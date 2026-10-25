# QQ 官方机器人适配器

对接 **QQ 开放平台**（bot.q.qq.com）的官方机器人接口，和 `OneBot11 + NapCat` 是并列的两套实现：

| 能力 | OneBot11 适配器 | QQ 官方适配器 |
| --- | --- | --- |
| 收事件 | NapCat 通过 HTTP POST 上报，框架被动接收 | 框架主动连 `wss` 网关 |
| 发消息 | NapCat 的 `/send_group_msg` 等 | 官方 REST `/v2/groups/{openid}/messages` |
| 用户标识 | QQ 号（数字） | `openid`（字符串） |
| 鉴权 | 无（本地 NapCat） | AppID + ClientSecret 换取 `access_token` |

## 启用

```shell
./slandbot --adapter qqbot --appid <AppID> --secret <ClientSecret>
```

也可以走环境变量，避免密钥留在命令行历史里：

```shell
export QQ_BOT_APPID=你的AppID
export QQ_BOT_SECRET=你的ClientSecret
./slandbot --adapter qqbot
```

可选参数：

| 参数 | 说明 |
| --- | --- |
| `--adapter <onebot11\|qqbot>` | 选择适配器，默认 `onebot11` |
| `--appid <id>` | 机器人 AppID |
| `--secret <secret>` | 机器人 ClientSecret |
| `--sandbox` | 使用沙箱环境 `sandbox.api.sgroup.qq.com` |

不指定 `--adapter` 时行为保持不变，仍然走 OneBot11 + NapCat。

## 实现说明

- `Server_QQBotGateway`：连官方网关，处理 `Hello / Identify / Heartbeat / READY`，
  并把 `GROUP_AT_MESSAGE_CREATE`、`C2C_MESSAGE_CREATE` 转成框架的 `MessageContent`
- `QQBotClient`：access_token 自动缓存（提前 2 分钟刷新），发送群聊/单聊消息
- `WebSocketClient`：最小 RFC6455 客户端（OpenSSL TLS），项目里的 httplib 不支持 WebSocket
- `QQBotCommon`：openid ↔ 框架 `long long` ID 的双向映射，以及被动回复窗口

### openid 映射

官方 API 里用户和群都是 `openid` 字符串，而框架的 `SendID` 用的是 `long long`。
适配器内部用 FNV-1a 折成 63 位正整数并维护双向表：

- 插件拿到的是稳定的**进程内**数字 ID
- 想拿回 openid：`qqbot::OpenIdOf(id)`
- **重启后映射会重建**，插件不要把数字 ID 当持久化标识

### 被动回复

适配器会记录每个会话最近一条消息的 `msg_id`，发送时自动带上。官方规则（消息收发概述，2026-07 修订）：

| 场景 | 被动消息有效期 | 同一 msg_id 最多回复 |
| --- | --- | --- |
| 单聊 | 60 分钟 | 4 次 |
| 群聊 | 5 分钟 | 5 次 |

这几个数字已经抽成配置。平台调整规则时用环境变量覆盖即可，不必改代码重编：

| 环境变量 | 默认 | 说明 |
| --- | --- | --- |
| `QQ_BOT_GROUP_REPLY_WINDOW` | 300 | 群聊被动窗口（秒） |
| `QQ_BOT_PRIVATE_REPLY_WINDOW` | 3600 | 单聊被动窗口（秒） |
| `QQ_BOT_GROUP_MAX_REPLY` | 5 | 群聊同一 msg_id 最多回复次数 |
| `QQ_BOT_PRIVATE_MAX_REPLY` | 4 | 单聊同一 msg_id 最多回复次数 |

超出窗口或次数后退化成主动消息。主动消息的现行额度比早期宽松得多：
- 单聊：约 1000 条/天/用户
- 群聊：约 1000 条/天/群
- 另有 QPS/QPM 频控（群 60/qpm 或未认证 30/qpm，单关系 20/qpm）

另外用户可以在 QQ 客户端关闭「允许主动发送」，关闭后主动消息一律失败，
所以重要的回复仍然应该尽量走被动消息。

## 当前限制

- **没有群成员列表**：官方 API 不提供，`GetGroupMemberList` 固定返回 `retcode = -1`
- **只发文本**：图片/语音/视频等媒体消息尚未实现（`msg_type` 恒为 0）。
  官方支持的类型是 `0` 文本、`2` Markdown、`7` 富媒体（需先上传拿 `file_info`）
- **频道消息不处理**：`AT_MESSAGE_CREATE` / `DIRECT_MESSAGE_CREATE` 走另一套
  `channel_id` 接口，当前只支持群聊和单聊
- 机器人需要先在开放平台配置好沙箱/正式环境的权限与事件订阅

## 调试

| 环境变量 | 作用 |
| --- | --- |
| `QQ_BOT_API_HOST` | 覆盖 API 域名，便于指向本地假网关联调 |
| `QQ_BOT_ACCESS_TOKEN` | 直接指定 token，跳过换取流程（调试用） |
| `SLB_INSECURE_TLS=1` | 跳过 TLS 证书校验（本机自签证书 / CA 缺失时用） |

`QQ_BOT_ACCESS_TOKEN` 只适合调试，正式环境请用 AppID + ClientSecret 让适配器自己续期。
