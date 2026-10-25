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

官方对主动推送有额度限制，带上 `msg_id` 的「被动回复」才不占额度。
适配器会记录每个会话最近一条消息的 `msg_id`，发送时自动带上（5 分钟窗口、同一
`msg_id` 最多回 5 次），超出后退化成主动推送。

## 当前限制

- **没有群成员列表**：官方 API 不提供，`GetGroupMemberList` 固定返回 `retcode = -1`
- **只发文本**：图片/语音/视频等媒体消息尚未实现（`msg_type` 恒为 0）
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
