project(L2AuthD)
cmake_minimum_required(VERSION 2.8)

set(HEADER_FILES
    ./src/AppInstance.h
    ./src/CIOObject.h
    ./src/config/CIPList.h
    ./src/config/Config.h
    ./src/crypt/Blowfish.h
    ./src/crypt/DesBackEnd.h
    ./src/crypt/DesFrontEnd.h
    ./src/crypt/DesNewCrypt.h
    ./src/crypt/OldCrypt.h
    ./src/crypt/PwdCrypt.h
    ./src/db/CAccount.h
    ./src/db/CDBConn.h
    ./src/db/CServerUserCountStatus.h
    ./src/db/DBEnv.h
    ./src/db/SqlConnection.h
    ./src/logger/CFileLog.h
    ./src/logger/CLogdFilelog.h
    ./src/logger/LogSeverity.h
    ./src/model/AccountDB.h
    ./src/model/CIPAccessLimit.h
    ./src/model/CServerKickList.h
    ./src/model/CServerList.h
    ./src/model/FrameType.h
    ./src/model/LoginFailReason.h
    ./src/model/LoginState.h
    ./src/model/LoginUser.h
    ./src/model/PlayFail.h
    ./src/model/ServerKind.h
    ./src/model/ServerPacketList.h
    ./src/model/ServersProvider.h
    ./src/model/SexAndCentury.h
    ./src/model/WorldServer.h
    ./src/network/BufferReader.h
    ./src/network/CAuthOverlapped.h
    ./src/network/CAuthOverlappedPool.h
    ./src/network/CAuthOverlappedSlot.h
    ./src/network/CAuthPacket.h
    ./src/network/CAuthPacketPool.h
    ./src/network/CAuthPacketSlot.h
    ./src/network/CAuthServer.h
    ./src/network/CAuthSocket.h
    ./src/network/CIOBuffer.h
    ./src/network/CIOBufferPool.h
    ./src/network/CIOBufferSlot.h
    ./src/network/CIOServer.h
    ./src/network/CIOServerInt.h
    ./src/network/CIOSocket.h
    ./src/network/CLogPacket.h
    ./src/network/CLogPacketPool.h
    ./src/network/CLogPacketSlot.h
    ./src/network/CLogSocket.h
    ./src/network/CSocketInt.h
    ./src/network/CWantedPacket.h
    ./src/network/CWantedPacketPool.h
    ./src/network/CWantedPacketSlot.h
    ./src/network/CWantedSocket.h
    ./src/network/IPPacket.h
    ./src/network/IPPacketPool.h
    ./src/network/IPPacketSlot.h
    ./src/network/IPSessionDB.h
    ./src/network/IPSocket.h
    ./src/network/PacketUtils.h
    ./src/network/SocketStatus.h
    ./src/network/WorldSrvPacket.h
    ./src/network/WorldSrvPacketPool.h
    ./src/network/WorldSrvPacketSlot.h
    ./src/network/WorldSrvServer.h
    ./src/network/WorldSrvSocket.h
    ./src/network/packets/LoginPackets.h
    ./src/threads/CIOTimer.h
    ./src/threads/CIOTopTimer.h
    ./src/threads/CJob.h
    ./src/threads/CLock.h
    ./src/threads/CRWLock.h
    ./src/threads/Threading.h
    ./src/ui/CLog.h
    ./src/ui/CReporter.h
    ./src/ui/MainWindow.h
    ./src/ui/resources.h
    ./src/utils/CExceptionInit.h
    ./src/utils/CoreDump.h
    ./src/utils/cstdint_support.h
    ./src/utils/SendMail.h
    ./src/utils/Unused.h
    ./src/utils/Utils.h
)

set(SOURCE_FILES
    ./src/CIOObject.cpp
    ./src/main.cpp
    ./src/config/CIPList.cpp
    ./src/config/Config.cpp
    ./src/crypt/Blowfish.cpp
    ./src/crypt/DesBackEnd.cpp
    ./src/crypt/DesFrontEnd_foreign.cpp
    ./src/crypt/DesNewCrypt.cpp
    ./src/crypt/OldCrypt.cpp
    ./src/crypt/PwdCrypt.cpp
    ./src/db/CAccount.cpp
    ./src/db/CDBConn.cpp
    ./src/db/CServerUserCountStatus.cpp
    ./src/db/DBEnv.cpp
    ./src/logger/CFileLog.cpp
    ./src/logger/CLogdFilelog.cpp
    ./src/logger/LogSeverity.cpp
    ./src/model/AccountDB.cpp
    ./src/model/CIPAccessLimit.cpp
    ./src/model/CServerKickList.cpp
    ./src/model/CServerList.cpp
    ./src/model/LoginFailReason.cpp
    ./src/model/LoginState.cpp
    ./src/model/LoginUser.cpp
    ./src/model/PlayFail.cpp
    ./src/model/ServerKind.cpp
    ./src/model/ServerPacketList.cpp
    ./src/model/ServersProvider.cpp
    ./src/model/SexAndCentury.cpp
    ./src/model/WorldServer.cpp
    ./src/network/BufferReader.cpp
    ./src/network/CAuthOverlapped.cpp
    ./src/network/CAuthOverlappedPool.cpp
    ./src/network/CAuthOverlappedSlot.cpp
    ./src/network/CAuthPacket.cpp
    ./src/network/CAuthPacketPool.cpp
    ./src/network/CAuthPacketSlot.cpp
    ./src/network/CAuthServer.cpp
    ./src/network/CAuthSocket.cpp
    ./src/network/CIOBuffer.cpp
    ./src/network/CIOBufferPool.cpp
    ./src/network/CIOBufferSlot.cpp
    ./src/network/CIOServer.cpp
    ./src/network/CIOServerInt.cpp
    ./src/network/CIOSocket.cpp
    ./src/network/CLogPacket.cpp
    ./src/network/CLogPacketPool.cpp
    ./src/network/CLogPacketSlot.cpp
    ./src/network/CLogSocket.cpp
    ./src/network/CSocketInt.cpp
    ./src/network/CWantedPacket.cpp
    ./src/network/CWantedPacketPool.cpp
    ./src/network/CWantedPacketSlot.cpp
    ./src/network/CWantedSocket.cpp
    ./src/network/IPPacket.cpp
    ./src/network/IPPacketPool.cpp
    ./src/network/IPPacketSlot.cpp
    ./src/network/IPSessionDB.cpp
    ./src/network/IPSocket.cpp
    ./src/network/PacketUtils.cpp
    ./src/network/SocketStatus.cpp
    ./src/network/WorldSrvPacket.cpp
    ./src/network/WorldSrvPacketPool.cpp
    ./src/network/WorldSrvPacketSlot.cpp
    ./src/network/WorldSrvServer.cpp
    ./src/network/WorldSrvSocket.cpp
    ./src/network/packets/LoginPackets.cpp
    ./src/threads/CIOTimer.cpp
    ./src/threads/CIOTopTimer.cpp
    ./src/threads/CJob.cpp
    ./src/threads/CLock.cpp
    ./src/threads/CRWLock.cpp
    ./src/threads/Threading.cpp
    ./src/ui/CLog.cpp
    ./src/ui/CReporter.cpp
    ./src/ui/MainWindow.cpp
    ./src/utils/CExceptionInit.cpp
    ./src/utils/CoreDump.cpp
    ./src/utils/SendMail.cpp
    ./src/utils/Utils.cpp
)

set(RC_FILE ./src/ui/resources.rc)

set(RESOURCES
    ./src/ui/icon.ico
    ./src/ui/icon_16x16.ico
)

set(CONFIGS
    ./etc/BlockIPs.txt
    ./etc/config.txt
    ./etc/serverlist
)

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

include_directories(./src)

add_executable(${PROJECT_NAME} WIN32  ${SOURCE_FILES} ${HEADER_FILES} ${RC_FILE} ${RESOURCES} ${CONFIGS})

target_link_libraries(${PROJECT_NAME}
    User32.lib
    Gdi32.lib    # Painting
    Ws2_32.lib   # Sockets
    Advapi32.lib # Regestry
    odbc32.lib   # SQL
    Mswsock.lib  # GetAcceptExSockaddrs
    Dbghelp.lib  # Debug symbol options
)

set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /DNDEBUG /MT")

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "General Flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Release Flags: ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "Debug Flags: ${CMAKE_CXX_FLAGS_DEBUG}")

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "${CMAKE_BINARY_DIR}/install/")
install(DIRECTORY "./etc" DESTINATION "${CMAKE_BINARY_DIR}/install/")
message(STATUS "Install path: ${CMAKE_BINARY_DIR}/install/")
