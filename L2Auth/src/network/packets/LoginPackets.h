#pragma once

enum LoginPackets
{
    LS_Init = 0x0,
    LS_LoginFail = 0x1,
    LS_AccountKicked = 0x2,
    LS_LoginOk = 0x3,
    LS_ServerList = 0x4,
    LS_ServerListFail = 0x5,
    LS_PlayFail = 0x6,
    LS_PlayOk = 0x7,
    LS_KickedFromGS = 0x8,
    LS_AccountBlocked = 0x9,
    LS_CharManipulation = 0xA,
    LS_CharSelection = 0xB,
    LS_CharDeleted = 0xC,
    LS_ServerSelection = 0xD,
};
