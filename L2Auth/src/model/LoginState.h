#pragma once

enum LoginState
{
    LoginState_Initial = 0x0,
    LoginState_Connected = 0x1,
    LoginState_AcceptedByGS = 0x3,
    LoginState_LoggedToGS = 0x4
};
