#pragma once

enum SpecialType
{
Message_Tirgger=1,
Message_Normal=2
};

enum WorkerType
{
    CommandRegister = 1,
    getAccountInfo = 2
};

//内部统计使用
enum iFunctionType
{
init=1,
filter=2,
action=3,
message=4,
notice=5
};