#pragma once

enum FrameType
{
    FrameType_DefaultOrCompressed = 0x0,
    FrameType_GameClient = 0x1,
    FrameType_Extended = 0x2,
    FrameType_WithKind = 0x3,
    FrameType_Compressed = 0x4,
};
