# -*- coding: utf-8 -*-
"""Our edits to the mt2009 client's C++ (the package's Source Client).

Usage:  python clientify.py <Source Client dir>

The client's source is not in this repository any more than the server's is
(linux-port/fetch-sources.sh): the package ships it, and what is ours is this
list of exact-string edits, applied the way playerbotify.py applies the
server's - idempotent, found already applied, or failing on the anchor it
could not find. The client's files mix CRLF and LF, so an anchor that spans
lines is tried with both, and a one-line anchor takes the line end of the line
it sits on. tools/build-client.ps1 copies the package source to a short path
(MSBuild cannot open "..\\UserInterface\\Locale_inc.h" under a long one), links
the package's Extern and our staged server tree beside it - the client reads
the server's common/*.h, which is where INVENTORY_PAGE_COUNT and the feature
switches come from - runs this, and builds.

What the edits are:
  personality row   textTail.AttachPersonality / DetachPersonality: a bot's
                    personality on a row of its own between its name and its
                    guild. client-root/playerbot_status_tail.py calls it (with
                    hasattr, so a client without it draws nothing). l0st3k
                    wrote it first for client 2.0.13 in a tree we do not have;
                    this is the same interface rebuilt on AttachTitle's model.
  discord presence  l0st3k's Discord application (its name and images are
                    what a friend's Discord shows beside the player) and the
                    button's address - the two strings his exe differs in from
                    the package's, read out of both binaries; the map table and
                    the texts are the package's in both.
  four pages        the client's half of playerbotify.py's
                    apply_four_inventory_pages. The slot map itself comes from
                    the server's common/length.h, which this build compiles
                    against; what is left is the belt's cells passing 255: a
                    quickslot's position and a shop sale's cell are WORDs on
                    the wire now, as on the server.
  co-op game host   the world is entered and every warp followed at the host
                    the player logged in through, with only the port taken
                    from the server: the server's one PROXY_IP cannot be both
                    a friend's route to the host and the host's own 127.0.0.1.
"""
import io
import os
import sys


def read(path):
    with io.open(path, 'rb') as f:
        return f.read()


def write(path, data):
    with io.open(path, 'wb') as f:
        f.write(data)


def edit(path, old, new, marker):
    data = read(path)
    name = os.path.basename(path)
    for eol in (b'\r\n', b'\n'):
        if marker.encode('latin-1').replace(b'\n', eol) in data:
            print('  already: %s' % name)
            return
    old_raw = old.encode('latin-1')
    for eol in (b'\r\n', b'\n'):
        old_b = old_raw.replace(b'\n', eol)
        if data.count(old_b) != 1:
            continue
        at = data.find(old_b)
        if b'\n' not in old_raw:
            end = data.find(b'\n', at)
            eol = b'\r\n' if end > 0 and data[end - 1:end] == b'\r' else b'\n'
        new_b = new.encode('latin-1').replace(b'\n', eol)
        write(path, data[:at] + new_b + data[at + len(old_b):])
        print('  edited:  %s' % name)
        return
    raise SystemExit('clientify: anchor not found exactly once in %s:\n%s' % (path, old))


def apply_personality_row(ui):
    header = os.path.join(ui, 'PythonTextTail.h')
    source = os.path.join(ui, 'PythonTextTail.cpp')
    module = os.path.join(ui, 'PythonTextTailModule.cpp')

    edit(header,
         '\t\t\tCGraphicTextInstance*\t\t\tpLevelTextInstance;',
         '\t\t\tCGraphicTextInstance*\t\t\tpLevelTextInstance;\n'
         '\t\t\t// A playerbot\'s personality, a row of its own between the name and\n'
         '\t\t\t// the guild (clientify.py; textTail.AttachPersonality).\n'
         '\t\t\tCGraphicTextInstance*\t\t\tpPersonalityTextInstance;',
         marker='pPersonalityTextInstance;')
    edit(header,
         '\t\tvoid DetachLevel(DWORD dwVID);',
         '\t\tvoid DetachLevel(DWORD dwVID);\n'
         '\n'
         '\t\tvoid AttachPersonality(DWORD dwVID, const char * c_szText, const D3DXCOLOR& c_rColor);\n'
         '\t\tvoid DetachPersonality(DWORD dwVID);',
         marker='void AttachPersonality(DWORD dwVID')

    # Every text tail is born without the row: the character's and the plain one.
    edit(source,
         '\tpTextTail->pLevelTextInstance=NULL;',
         '\tpTextTail->pLevelTextInstance=NULL;\n'
         '\tpTextTail->pPersonalityTextInstance=NULL;',
         marker='pTextTail->pPersonalityTextInstance=NULL;')
    edit(source,
         '\tpTextTail->pLevelTextInstance = NULL;\n\treturn pTextTail;',
         '\tpTextTail->pLevelTextInstance = NULL;\n'
         '\tpTextTail->pPersonalityTextInstance = NULL;\n'
         '\treturn pTextTail;',
         marker='pTextTail->pPersonalityTextInstance = NULL;\n\treturn pTextTail;')
    edit(source,
         '\tm_TextTailPool.Free(pTextTail);',
         '\tif (pTextTail->pPersonalityTextInstance)\n'
         '\t{\n'
         '\t\tCGraphicTextInstance::Delete(pTextTail->pPersonalityTextInstance);\n'
         '\t\tpTextTail->pPersonalityTextInstance = NULL;\n'
         '\t}\n'
         '\n'
         '\tm_TextTailPool.Free(pTextTail);',
         marker='CGraphicTextInstance::Delete(pTextTail->pPersonalityTextInstance);')

    # The row stands where the guild name would, and the guild goes up a row.
    edit(source,
         '\t\tCGraphicTextInstance * pGuildNameInstance = pTextTail->pGuildNameTextInstance;',
         '\t\t// The playerbot personality row (clientify.py) stands where the guild\n'
         '\t\t// name would, and the guild name and its mark go up a row above it.\n'
         '\t\tfloat fyPersonalityShift = 0.0f;\n'
         '\t\tCGraphicTextInstance * pPersonality = pTextTail->pPersonalityTextInstance;\n'
         '\t\tif (pPersonality)\n'
         '\t\t{\n'
         '\t\t\tpPersonality->SetPosition(pTextTail->x, pTextTail->y - c_fyGuildNamePosition, pTextTail->z);\n'
         '\t\t\tpPersonality->Update();\n'
         '\t\t\tfyPersonalityShift = c_fyGuildNamePosition;\n'
         '\t\t}\n'
         '\n'
         '\t\tCGraphicTextInstance * pGuildNameInstance = pTextTail->pGuildNameTextInstance;',
         marker='float fyPersonalityShift = 0.0f;')
    edit(source,
         'pMarkInstance->SetPosition(pTextTail->x - iWidth/2 - iImageHalfSize, pTextTail->y - c_fyMarkPosition);',
         'pMarkInstance->SetPosition(pTextTail->x - iWidth/2 - iImageHalfSize, pTextTail->y - c_fyMarkPosition - fyPersonalityShift);',
         marker='c_fyMarkPosition - fyPersonalityShift);')
    edit(source,
         'pGuildNameInstance->SetPosition(pTextTail->x + iImageHalfSize, pTextTail->y - c_fyGuildNamePosition, pTextTail->z);',
         'pGuildNameInstance->SetPosition(pTextTail->x + iImageHalfSize, pTextTail->y - c_fyGuildNamePosition - fyPersonalityShift, pTextTail->z);',
         marker='c_fyGuildNamePosition - fyPersonalityShift, pTextTail->z);')

    # Drawn after the level, inside the same character loop.
    edit(source,
         '\t\t\tpTextTail->pLevelTextInstance->Render();',
         '\t\t\tpTextTail->pLevelTextInstance->Render();\n'
         '\t\t}\n'
         '\t\tif (pTextTail->pPersonalityTextInstance)\n'
         '\t\t{\n'
         '\t\t\tpTextTail->pPersonalityTextInstance->Render();',
         marker='pTextTail->pPersonalityTextInstance->Render();')

    edit(source,
         'void CPythonTextTail::Initialize()',
         '// A playerbot\'s personality on a row of its own (clientify.py). It is no\n'
         '// PK information, so EnablePKTitle does not hide it: the player\'s switch is\n'
         '// in playerbot_status_tail.py, which calls this every second for the bots it\n'
         '// has heard of - so the instance is made once and only its text and colour\n'
         '// change after that.\n'
         'void CPythonTextTail::AttachPersonality(DWORD dwVID, const char * c_szText, const D3DXCOLOR & c_rColor)\n'
         '{\n'
         '\tTTextTailMap::iterator itor = m_CharacterTextTailMap.find(dwVID);\n'
         '\tif (m_CharacterTextTailMap.end() == itor)\n'
         '\t\treturn;\n'
         '\n'
         '\tTTextTail * pTextTail = itor->second;\n'
         '\n'
         '\tCGraphicTextInstance *& prPersonality = pTextTail->pPersonalityTextInstance;\n'
         '\tif (!prPersonality)\n'
         '\t{\n'
         '\t\tprPersonality = CGraphicTextInstance::New();\n'
         '\t\tprPersonality->SetTextPointer(ms_pFont);\n'
         '\t\tprPersonality->SetOutline(true);\n'
         '\t\tprPersonality->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_CENTER);\n'
         '\t\tprPersonality->SetVerticalAlign(CGraphicTextInstance::VERTICAL_ALIGN_BOTTOM);\n'
         '\t}\n'
         '\n'
         '\tprPersonality->SetValue(c_szText);\n'
         '\tprPersonality->SetColor(c_rColor.r, c_rColor.g, c_rColor.b);\n'
         '\tprPersonality->Update();\n'
         '}\n'
         '\n'
         'void CPythonTextTail::DetachPersonality(DWORD dwVID)\n'
         '{\n'
         '\tTTextTailMap::iterator itor = m_CharacterTextTailMap.find(dwVID);\n'
         '\tif (m_CharacterTextTailMap.end() == itor)\n'
         '\t\treturn;\n'
         '\n'
         '\tTTextTail * pTextTail = itor->second;\n'
         '\n'
         '\tif (pTextTail->pPersonalityTextInstance)\n'
         '\t{\n'
         '\t\tCGraphicTextInstance::Delete(pTextTail->pPersonalityTextInstance);\n'
         '\t\tpTextTail->pPersonalityTextInstance = NULL;\n'
         '\t}\n'
         '}\n'
         '\n'
         'void CPythonTextTail::Initialize()',
         marker='void CPythonTextTail::AttachPersonality(')

    edit(module,
         'PyObject * textTailShowCharacterTextTail(PyObject * poSelf, PyObject * poArgs)',
         '// textTail.AttachPersonality(vid, text, r, g, b) and DetachPersonality(vid):\n'
         '// a playerbot\'s personality on its own row (clientify.py).\n'
         'PyObject * textTailAttachPersonality(PyObject * poSelf, PyObject * poArgs)\n'
         '{\n'
         '\tint iVirtualID;\n'
         '\tif (!PyTuple_GetInteger(poArgs, 0, &iVirtualID))\n'
         '\t\treturn Py_BuildException();\n'
         '\tchar * szText;\n'
         '\tif (!PyTuple_GetString(poArgs, 1, &szText))\n'
         '\t\treturn Py_BuildException();\n'
         '\tfloat fr;\n'
         '\tif (!PyTuple_GetFloat(poArgs, 2, &fr))\n'
         '\t\treturn Py_BuildException();\n'
         '\tfloat fg;\n'
         '\tif (!PyTuple_GetFloat(poArgs, 3, &fg))\n'
         '\t\treturn Py_BuildException();\n'
         '\tfloat fb;\n'
         '\tif (!PyTuple_GetFloat(poArgs, 4, &fb))\n'
         '\t\treturn Py_BuildException();\n'
         '\n'
         '\tCPythonTextTail::Instance().AttachPersonality(iVirtualID, szText, D3DXCOLOR(fr, fg, fb, 1.0f));\n'
         '\treturn Py_BuildNone();\n'
         '}\n'
         '\n'
         'PyObject * textTailDetachPersonality(PyObject * poSelf, PyObject * poArgs)\n'
         '{\n'
         '\tint iVirtualID;\n'
         '\tif (!PyTuple_GetInteger(poArgs, 0, &iVirtualID))\n'
         '\t\treturn Py_BuildException();\n'
         '\n'
         '\tCPythonTextTail::Instance().DetachPersonality(iVirtualID);\n'
         '\treturn Py_BuildNone();\n'
         '}\n'
         '\n'
         'PyObject * textTailShowCharacterTextTail(PyObject * poSelf, PyObject * poArgs)',
         marker='PyObject * textTailAttachPersonality(')
    edit(module,
         '\t\t{ "AttachTitle",\t\t\t\ttextTailAttachTitle,\t\t\t\tMETH_VARARGS },',
         '\t\t{ "AttachTitle",\t\t\t\ttextTailAttachTitle,\t\t\t\tMETH_VARARGS },\n'
         '\t\t{ "AttachPersonality",\t\ttextTailAttachPersonality,\t\tMETH_VARARGS },\n'
         '\t\t{ "DetachPersonality",\t\ttextTailDetachPersonality,\t\tMETH_VARARGS },',
         marker='{ "AttachPersonality",')


def apply_discord_presence(ui):
    # The application id decides the name Discord prints over the presence
    # ("Mt2009" for the package's) and which uploaded images race_N and
    # empire_N resolve to; l0st3k's application carries both.
    edit(os.path.join(ui, 'Discord.h'),
         'constexpr auto DiscordClientID = "1180989036949680258";',
         'constexpr auto DiscordClientID = "1548716643541065798";',
         marker='DiscordClientID = "1548716643541065798";')
    edit(os.path.join(ui, 'PythonNetworkStreamPhaseGame.cpp'),
         'discordPresence.buttonURL = "https://mt2009.pl/";',
         'discordPresence.buttonURL = "https://www.youtube.com/@tieru/";',
         marker='discordPresence.buttonURL = "https://www.youtube.com/@tieru/";')


def apply_four_inventory_pages(ui):
    # The belt runs to 302 with four pages, and a quickslot names a belt cell
    # as readily as a bag one: the position is a WORD in TQuickSlot and in
    # every hand it passes through (AddQuickSlot took a signed char).
    edit(os.path.join(ui, 'GameType.h'),
         'typedef struct SQuickSlot\n{\n\tBYTE Type;\n\tBYTE Position;\n} TQuickSlot;',
         'typedef struct SQuickSlot\n{\n\tBYTE Type;\n'
         '\t// A WORD since the four inventory pages (clientify.py).\n'
         '\tWORD Position;\n} TQuickSlot;',
         marker='\tWORD Position;\n} TQuickSlot;')
    # The network stream calls it through IAbstractPlayer, which declares it.
    edit(os.path.join(ui, 'AbstractPlayer.h'),
         'virtual void\tAddQuickSlot(int QuickslotIndex, char IconType, char IconPosition) = 0;',
         'virtual void\tAddQuickSlot(int QuickslotIndex, char IconType, WORD IconPosition) = 0;',
         marker='virtual void\tAddQuickSlot(int QuickslotIndex, char IconType, WORD IconPosition) = 0;')
    edit(os.path.join(ui, 'PythonPlayer.h'),
         'void\tAddQuickSlot(int QuickslotIndex, char IconType, char IconPosition);',
         'void\tAddQuickSlot(int QuickslotIndex, char IconType, WORD IconPosition);',
         marker='void\tAddQuickSlot(int QuickslotIndex, char IconType, WORD IconPosition);')
    player = os.path.join(ui, 'PythonPlayer.cpp')
    edit(player,
         'void CPythonPlayer::AddQuickSlot(int QuickSlotIndex, char IconType, char IconPosition)',
         'void CPythonPlayer::AddQuickSlot(int QuickSlotIndex, char IconType, WORD IconPosition)',
         marker='void CPythonPlayer::AddQuickSlot(int QuickSlotIndex, char IconType, WORD IconPosition)')
    edit(player,
         '(BYTE)dwGlobalSlotIndex, (BYTE)dwWndType, (BYTE)dwWndItemPos);',
         '(BYTE)dwGlobalSlotIndex, (BYTE)dwWndType, (WORD)dwWndItemPos);',
         marker='(BYTE)dwGlobalSlotIndex, (BYTE)dwWndType, (WORD)dwWndItemPos);')
    edit(player,
         '(BYTE)dwGlobalQuickSlotIndex, (BYTE)dwWndType, (BYTE)dwWndItemPos);',
         '(BYTE)dwGlobalQuickSlotIndex, (BYTE)dwWndType, (WORD)dwWndItemPos);',
         marker='(BYTE)dwGlobalQuickSlotIndex, (BYTE)dwWndType, (WORD)dwWndItemPos);')
    stream_h = os.path.join(ui, 'PythonNetworkStream.h')
    item_cpp = os.path.join(ui, 'PythonNetworkStreamPhaseGameItem.cpp')
    edit(stream_h,
         'bool SendQuickSlotAddPacket(BYTE wpos, BYTE type, BYTE pos);',
         'bool SendQuickSlotAddPacket(BYTE wpos, BYTE type, WORD pos);',
         marker='bool SendQuickSlotAddPacket(BYTE wpos, BYTE type, WORD pos);')
    edit(item_cpp,
         'bool CPythonNetworkStream::SendQuickSlotAddPacket(BYTE wpos, BYTE type, BYTE pos)',
         'bool CPythonNetworkStream::SendQuickSlotAddPacket(BYTE wpos, BYTE type, WORD pos)',
         marker='bool CPythonNetworkStream::SendQuickSlotAddPacket(BYTE wpos, BYTE type, WORD pos)')

    # A shop sale's cell: in a byte, a belt potion sold whatever lay on the
    # bag cell 256 below it. Send() writes the parameter's own size, so the
    # wire is two bytes now, which the server reads (input_main.cpp, SELL2).
    edit(stream_h,
         'bool SendShopSellPacketNew(BYTE bySlot, ITEM_COUNT byCount);',
         'bool SendShopSellPacketNew(WORD bySlot, ITEM_COUNT byCount);',
         marker='bool SendShopSellPacketNew(WORD bySlot, ITEM_COUNT byCount);')
    edit(item_cpp,
         'bool CPythonNetworkStream::SendShopSellPacketNew(BYTE bySlot, ITEM_COUNT byCount)',
         'bool CPythonNetworkStream::SendShopSellPacketNew(WORD bySlot, ITEM_COUNT byCount)',
         marker='bool CPythonNetworkStream::SendShopSellPacketNew(WORD bySlot, ITEM_COUNT byCount)')


def apply_coop_game_host(ui):
    # The server names one address for every client: each core's PROXY_IP in
    # the character list (LOGIN_SUCCESS) and in every warp, 127.0.0.1 on a
    # single-player install. A friend who logged in through the host's public
    # address would be sent to his own 127.0.0.1 on entering the world. Every
    # core of one world stands behind the address the player logged in
    # through, so the client keeps that address (the chosen server's host in
    # serverinfo.py) and takes only the port from the server. On the host's
    # own machine that address is 127.0.0.1, which is also what makes a
    # router without NAT loopback irrelevant.
    stream_h = os.path.join(ui, 'PythonNetworkStream.h')
    edit(stream_h,
         '\t\tvoid ConnectGameServer(UINT iChrSlot);\n',
         '\t\tvoid ConnectGameServer(UINT iChrSlot);\n'
         '\t\t// The host the player logged in through; the world is entered and\n'
         '\t\t// every warp followed there, the port alone being the server\'s\n'
         '\t\t// (clientify.py, co-op).\n'
         '\t\tvoid SetGameHost(const char* c_szHost);\n',
         marker='void SetGameHost(const char* c_szHost);')
    edit(stream_h,
         '\t\tstd::string\tm_stPassword;\n',
         '\t\tstd::string\tm_stPassword;\n'
         '\t\tstd::string\tm_stGameHost;\n',
         marker='\t\tstd::string\tm_stGameHost;')
    stream_cpp = os.path.join(ui, 'PythonNetworkStream.cpp')
    edit(stream_cpp,
         '\tTSimplePlayerInformation&\trkSimplePlayerInfo=m_akSimplePlayerInfo[iChrSlot];\n'
         '\tCNetworkStream::Connect((DWORD)rkSimplePlayerInfo.lAddr, rkSimplePlayerInfo.wPort);\n'
         '}\n',
         '\tTSimplePlayerInformation&\trkSimplePlayerInfo=m_akSimplePlayerInfo[iChrSlot];\n'
         '\tif (!m_stGameHost.empty())\n'
         '\t\tCNetworkStream::Connect(m_stGameHost.c_str(), rkSimplePlayerInfo.wPort);\n'
         '\telse\n'
         '\t\tCNetworkStream::Connect((DWORD)rkSimplePlayerInfo.lAddr, rkSimplePlayerInfo.wPort);\n'
         '}\n'
         '\n'
         'void CPythonNetworkStream::SetGameHost(const char* c_szHost)\n'
         '{\n'
         '\tm_stGameHost = c_szHost ? c_szHost : "";\n'
         '}\n',
         marker='void CPythonNetworkStream::SetGameHost(const char* c_szHost)')
    edit(os.path.join(ui, 'PythonNetworkStreamPhaseGame.cpp'),
         '\tCNetworkStream::PingPort(kWarpPacket.lAddr, kWarpPacket.wPort);\n'
         '\tSleep(2000);\n'
         '\tCNetworkStream::Connect((DWORD)kWarpPacket.lAddr, kWarpPacket.wPort);\n',
         '\t// The core\'s port from the server, the host the player logged in\n'
         '\t// through (SetGameHost, clientify.py).\n'
         '\tif (!m_stGameHost.empty())\n'
         '\t{\n'
         '\t\tCNetworkStream::PingPort(m_stGameHost, kWarpPacket.wPort);\n'
         '\t\tSleep(2000);\n'
         '\t\tCNetworkStream::Connect(m_stGameHost.c_str(), kWarpPacket.wPort);\n'
         '\t}\n'
         '\telse\n'
         '\t{\n'
         '\t\tCNetworkStream::PingPort(kWarpPacket.lAddr, kWarpPacket.wPort);\n'
         '\t\tSleep(2000);\n'
         '\t\tCNetworkStream::Connect((DWORD)kWarpPacket.lAddr, kWarpPacket.wPort);\n'
         '\t}\n',
         marker='CNetworkStream::PingPort(m_stGameHost, kWarpPacket.wPort);')
    edit(os.path.join(ui, 'AccountConnector.cpp'),
         '\t\trkNet.Connect(m_strAddr.c_str(), m_iPort);\n',
         '\t\trkNet.SetGameHost(m_strAddr.c_str());\n'
         '\t\trkNet.Connect(m_strAddr.c_str(), m_iPort);\n',
         marker='rkNet.SetGameHost(m_strAddr.c_str());')


AUTO_HUNT_CIRCLE_RENDER = (
    '// Auto Lowy 2.0 (Colide, 22 September): the hunt\'s range drawn on the\n'
    '// ground round the character, or - with "Wracaj" - round the point the\n'
    '// hunt returns to, with a cross on that point. Three rings of 120\n'
    '// segments that follow the terrain, the middle one pulsing. The window\n'
    '// sets the range (player.SetAutoHuntRangeCircle) and zero hides it. The\n'
    '// render states it changes are saved and put back, not set to fixed\n'
    '// values, so the passes after it draw as they did.\n'
    'static void RenderAutoHuntRangeCircle()\n'
    '{\n'
    '\tconst DWORD dwAutoHuntRange = CPythonPlayer::Instance().GetAutoHuntRangeCircle();\n'
    '\tif (dwAutoHuntRange == 0)\n'
    '\t\treturn;\n'
    '\tCInstanceBase* pMainInst = CPythonCharacterManager::Instance().GetMainInstancePtr();\n'
    '\tif (!pMainInst)\n'
    '\t\treturn;\n'
    '\n'
    '\tfloat fAnchorX = 0.0f, fAnchorY = 0.0f;\n'
    '\tbool bIsReturn = false;\n'
    '\tCPythonPlayer::Instance().GetAutoHuntRangeCirclePosition(&fAnchorX, &fAnchorY, &bIsReturn);\n'
    '\n'
    '\tfloat cx, cy;\n'
    '\tDWORD dwBaseColor;\n'
    '\tif (bIsReturn)\n'
    '\t{\n'
    '\t\tcx = fAnchorX;\n'
    '\t\tcy = -fAnchorY;\n'
    '\t\tdwBaseColor = 0x0000FF00;\n'
    '\t}\n'
    '\telse\n'
    '\t{\n'
    '\t\tconst D3DXVECTOR3& c_rv3Center = pMainInst->GetGraphicThingInstancePtr()->GetPosition();\n'
    '\t\tcx = c_rv3Center.x;\n'
    '\t\tcy = c_rv3Center.y;\n'
    '\t\tdwBaseColor = 0x0000BFFF;\n'
    '\t}\n'
    '\n'
    '\tconst float fTime = GetTickCount() / 1000.0f;\n'
    '\tconst float fPulse = (sinf(fTime * 3.0f) + 1.0f) * 0.5f;\n'
    '\tconst int iAlpha = 80 + (int)(fPulse * 100.0f);\n'
    '\tconst DWORD dwCircleColor = dwBaseColor | ((DWORD)iAlpha << 24);\n'
    '\tconst DWORD dwEdgeColor = dwBaseColor | ((DWORD)(iAlpha / 2) << 24);\n'
    '\n'
    '\tD3DXMATRIX matWorld;\n'
    '\tD3DXMatrixIdentity(&matWorld);\n'
    '\tSTATEMANAGER.SaveTransform(D3DTS_WORLD, &matWorld);\n'
    '\tSTATEMANAGER.SetTexture(0, NULL);\n'
    '\tSTATEMANAGER.SetTexture(1, NULL);\n'
    '\tSTATEMANAGER.SaveRenderState(D3DRS_ALPHABLENDENABLE, TRUE);\n'
    '\tSTATEMANAGER.SaveRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);\n'
    '\tSTATEMANAGER.SaveRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);\n'
    '\tSTATEMANAGER.SaveRenderState(D3DRS_LIGHTING, FALSE);\n'
    '\tSTATEMANAGER.SaveRenderState(D3DRS_ZENABLE, FALSE);\n'
    '\tSTATEMANAGER.SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);\n'
    '\n'
    '\tstruct FVF_XYZ_DIFFUSE\n'
    '\t{\n'
    '\t\tfloat x, y, z;\n'
    '\t\tDWORD diffuse;\n'
    '\t};\n'
    '\tconst int iSegments = 120;\n'
    '\tstatic std::vector<FVF_XYZ_DIFFUSE> s_vertices(iSegments + 1);\n'
    '\tconst float fRadii[3] = { (float)dwAutoHuntRange - 3.0f, (float)dwAutoHuntRange, (float)dwAutoHuntRange + 3.0f };\n'
    '\tfor (int r = 0; r < 3; ++r)\n'
    '\t{\n'
    '\t\tconst float fRadius = fRadii[r];\n'
    '\t\tfor (int i = 0; i <= iSegments; ++i)\n'
    '\t\t{\n'
    '\t\t\tconst float fAngle = D3DX_PI * 2.0f * (float)i / (float)iSegments;\n'
    '\t\t\tconst float x = cx + fRadius * cosf(fAngle);\n'
    '\t\t\tconst float y = cy - fRadius * sinf(fAngle);\n'
    '\t\t\ts_vertices[i].x = x;\n'
    '\t\t\ts_vertices[i].y = y;\n'
    '\t\t\ts_vertices[i].z = CPythonBackground::Instance().GetHeight(x, y) + 5.0f;\n'
    '\t\t\ts_vertices[i].diffuse = r == 1 ? dwCircleColor : dwEdgeColor;\n'
    '\t\t}\n'
    '\t\tSTATEMANAGER.DrawPrimitiveUP(D3DPT_LINESTRIP, iSegments, &s_vertices[0], sizeof(FVF_XYZ_DIFFUSE));\n'
    '\t}\n'
    '\tif (bIsReturn)\n'
    '\t{\n'
    '\t\tconst float az = CPythonBackground::Instance().GetHeight(cx, cy) + 5.0f;\n'
    '\t\tconst float as = 30.0f;\n'
    '\t\tFVF_XYZ_DIFFUSE anchor[4] = {\n'
    '\t\t\t{ cx - as, cy, az, dwCircleColor }, { cx + as, cy, az, dwCircleColor },\n'
    '\t\t\t{ cx, cy - as, az, dwCircleColor }, { cx, cy + as, az, dwCircleColor },\n'
    '\t\t};\n'
    '\t\tSTATEMANAGER.DrawPrimitiveUP(D3DPT_LINELIST, 2, &anchor[0], sizeof(FVF_XYZ_DIFFUSE));\n'
    '\t}\n'
    '\n'
    '\tSTATEMANAGER.RestoreRenderState(D3DRS_ZENABLE);\n'
    '\tSTATEMANAGER.RestoreRenderState(D3DRS_LIGHTING);\n'
    '\tSTATEMANAGER.RestoreRenderState(D3DRS_DESTBLEND);\n'
    '\tSTATEMANAGER.RestoreRenderState(D3DRS_SRCBLEND);\n'
    '\tSTATEMANAGER.RestoreRenderState(D3DRS_ALPHABLENDENABLE);\n'
    '\tSTATEMANAGER.RestoreTransform(D3DTS_WORLD);\n'
    '}\n'
    '\n'
)


def apply_auto_hunt_circle(ui):
    # Colide's Auto Lowy 2.0 (22 September): three functions for the window
    # (player.SetAutoHuntRangeCircle, IsBowEquipped, IsTargetDead) and the
    # range drawn on the ground. His own client files carried exactly these
    # changes on top of the package's; the render is one function called
    # after both of RenderGame's m_kChrMgr.Render() (the perf checker's and
    # the ordinary one) rather than the same block pasted twice.
    header = os.path.join(ui, 'PythonPlayer.h')
    edit(header,
         '\t\tDWORD\tGetPlayTime();\n'
         '\t\tvoid\tSetPlayTime(DWORD dwPlayTime);\n',
         '\t\tDWORD\tGetPlayTime();\n'
         '\t\tvoid\tSetPlayTime(DWORD dwPlayTime);\n'
         '\n'
         '\t\t// Auto Lowy 2.0 (Colide): the hunt\'s range drawn on the ground.\n'
         '\t\tvoid\tSetAutoHuntRangeCircle(DWORD dwRange, float fX = 0.0f, float fY = 0.0f, bool bIsReturn = false);\n'
         '\t\tDWORD\tGetAutoHuntRangeCircle();\n'
         '\t\tvoid\tGetAutoHuntRangeCirclePosition(float* pfX, float* pfY, bool* pbIsReturn);\n',
         marker='\t\tDWORD\tGetAutoHuntRangeCircle();\n')
    edit(header,
         '\t\tDWORD\t\t\t\t\tm_dwPlayTime;\n',
         '\t\tDWORD\t\t\t\t\tm_dwPlayTime;\n'
         '\t\t// Auto Lowy 2.0: the circle\'s range and, in "Wracaj", its fixed centre.\n'
         '\t\tDWORD\t\t\t\t\tm_dwAutoHuntRange;\n'
         '\t\tfloat\t\t\t\t\tm_fAutoHuntCircleX;\n'
         '\t\tfloat\t\t\t\t\tm_fAutoHuntCircleY;\n'
         '\t\tbool\t\t\t\t\tm_bAutoHuntCircleIsReturn;\n',
         marker='\t\tDWORD\t\t\t\t\tm_dwAutoHuntRange;\n')
    player = os.path.join(ui, 'PythonPlayer.cpp')
    edit(player,
         '\tm_dwPlayTime = 0;\n',
         '\tm_dwPlayTime = 0;\n'
         '\tm_dwAutoHuntRange = 0;\n'
         '\tm_fAutoHuntCircleX = 0.0f;\n'
         '\tm_fAutoHuntCircleY = 0.0f;\n'
         '\tm_bAutoHuntCircleIsReturn = false;\n',
         marker='\tm_dwAutoHuntRange = 0;\n')
    edit(player,
         "//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f",
         '// Auto Lowy 2.0 (Colide): the range circle the window asks the render for.\n'
         'void CPythonPlayer::SetAutoHuntRangeCircle(DWORD dwRange, float fX, float fY, bool bIsReturn)\n'
         '{\n'
         '\tm_dwAutoHuntRange = dwRange;\n'
         '\tm_fAutoHuntCircleX = fX;\n'
         '\tm_fAutoHuntCircleY = fY;\n'
         '\tm_bAutoHuntCircleIsReturn = bIsReturn;\n'
         '}\n'
         '\n'
         'DWORD CPythonPlayer::GetAutoHuntRangeCircle()\n'
         '{\n'
         '\treturn m_dwAutoHuntRange;\n'
         '}\n'
         '\n'
         'void CPythonPlayer::GetAutoHuntRangeCirclePosition(float* pfX, float* pfY, bool* pbIsReturn)\n'
         '{\n'
         '\t*pfX = m_fAutoHuntCircleX;\n'
         '\t*pfY = m_fAutoHuntCircleY;\n'
         '\t*pbIsReturn = m_bAutoHuntCircleIsReturn;\n'
         '}\n'
         '\n'
         "//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f",
         marker='void CPythonPlayer::SetAutoHuntRangeCircle(DWORD dwRange, float fX, float fY, bool bIsReturn)\n')
    module = os.path.join(ui, 'PythonPlayerModule.cpp')
    edit(module,
         'void initPlayer()\n{\n\tstatic PyMethodDef s_methods[] =\n',
         '// Auto Lowy 2.0 (Colide, 22 September), for client-root/uiautohunt.py:\n'
         '// the range circle, whether the hand holds a bow (the reach goes from\n'
         '// melee to the bow\'s), and whether a target is already dead in this\n'
         '// client - the server keeps a corpse for two or three seconds and\n'
         '// named it again as the nearest monster.\n'
         'PyObject* playerSetAutoHuntRangeCircle(PyObject* poSelf, PyObject* poArgs)\n'
         '{\n'
         '\tint iRange;\n'
         '\tif (!PyTuple_GetInteger(poArgs, 0, &iRange))\n'
         '\t\treturn Py_BuildException();\n'
         '\tfloat fX = 0.0f;\n'
         '\tfloat fY = 0.0f;\n'
         '\tint iIsReturn = 0;\n'
         '\tif (PyTuple_Size(poArgs) >= 3)\n'
         '\t{\n'
         '\t\tPyTuple_GetFloat(poArgs, 1, &fX);\n'
         '\t\tPyTuple_GetFloat(poArgs, 2, &fY);\n'
         '\t}\n'
         '\tif (PyTuple_Size(poArgs) >= 4)\n'
         '\t\tPyTuple_GetInteger(poArgs, 3, &iIsReturn);\n'
         '\tCPythonPlayer::Instance().SetAutoHuntRangeCircle((DWORD)(iRange > 0 ? iRange : 0), fX, fY, iIsReturn != 0);\n'
         '\treturn Py_BuildNone();\n'
         '}\n'
         '\n'
         'PyObject* playerIsBowEquipped(PyObject* poSelf, PyObject* poArgs)\n'
         '{\n'
         '\tCInstanceBase* pMainInst = CPythonCharacterManager::Instance().GetMainInstancePtr();\n'
         '\tif (!pMainInst)\n'
         '\t\treturn Py_BuildValue("i", 0);\n'
         '\treturn Py_BuildValue("i", pMainInst->IsBowMode() ? 1 : 0);\n'
         '}\n'
         '\n'
         'PyObject* playerIsTargetDead(PyObject* poSelf, PyObject* poArgs)\n'
         '{\n'
         '\tint iVID;\n'
         '\tif (!PyTuple_GetInteger(poArgs, 0, &iVID))\n'
         '\t\treturn Py_BuildException();\n'
         '\tCInstanceBase* pInstance = CPythonCharacterManager::Instance().GetInstancePtr(iVID);\n'
         '\tif (!pInstance)\n'
         '\t\treturn Py_BuildValue("i", 1);\n'
         '\treturn Py_BuildValue("i", pInstance->IsDead() ? 1 : 0);\n'
         '}\n'
         '\n'
         'void initPlayer()\n{\n\tstatic PyMethodDef s_methods[] =\n',
         marker='PyObject* playerSetAutoHuntRangeCircle(PyObject* poSelf, PyObject* poArgs)\n')
    edit(module,
         '\t\t{ "GetBonusSP",playerGetBonusSP,\t\t\t\t\tMETH_VARARGS },\n',
         '\t\t{ "GetBonusSP",playerGetBonusSP,\t\t\t\t\tMETH_VARARGS },\n'
         '\t\t{ "SetAutoHuntRangeCircle",\t\tplayerSetAutoHuntRangeCircle,\t\tMETH_VARARGS },\n'
         '\t\t{ "IsBowEquipped",\t\t\t\tplayerIsBowEquipped,\t\t\t\tMETH_VARARGS },\n'
         '\t\t{ "IsTargetDead",\t\t\t\tplayerIsTargetDead,\t\t\t\t\tMETH_VARARGS },\n',
         marker='\t\t{ "SetAutoHuntRangeCircle",\t\tplayerSetAutoHuntRangeCircle,')
    app = os.path.join(ui, 'PythonApplication.cpp')
    edit(app,
         '#include "PythonSystem.h"\n',
         '#include "PythonSystem.h"\n'
         '#include "../EterLib/StateManager.h"\n',
         marker='#include "../EterLib/StateManager.h"\n')
    edit(app,
         'void CPythonApplication::RenderGame()\n',
         AUTO_HUNT_CIRCLE_RENDER + 'void CPythonApplication::RenderGame()\n',
         marker='static void RenderAutoHuntRangeCircle()\n')
    edit(app,
         '\t\tm_pyBackground.SetCharacterDirLight();\n'
         '\t\tm_kChrMgr.Render();\n',
         '\t\tm_pyBackground.SetCharacterDirLight();\n'
         '\t\tm_kChrMgr.Render();\n'
         '\t\tRenderAutoHuntRangeCircle();\n',
         marker='\t\tm_kChrMgr.Render();\n\t\tRenderAutoHuntRangeCircle();\n')
    edit(app,
         '\tDWORD t10=ELTimer_GetMSec();\n'
         '\tm_kChrMgr.Render();\n',
         '\tDWORD t10=ELTimer_GetMSec();\n'
         '\tm_kChrMgr.Render();\n'
         '\tRenderAutoHuntRangeCircle();\n',
         marker='\tm_kChrMgr.Render();\n\tRenderAutoHuntRangeCircle();\n')


def main(root):
    ui = os.path.join(root, 'UserInterface')
    if not os.path.isfile(os.path.join(ui, 'PythonTextTail.cpp')):
        raise SystemExit('clientify: no UserInterface/PythonTextTail.cpp under %s' % root)
    print('clientify: %s' % root)
    apply_personality_row(ui)
    apply_discord_presence(ui)
    apply_four_inventory_pages(ui)
    apply_coop_game_host(ui)
    apply_auto_hunt_circle(ui)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    main(sys.argv[1])
