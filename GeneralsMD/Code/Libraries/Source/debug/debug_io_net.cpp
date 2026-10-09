/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/////////////////////////////////////////////////////////////////////////EA-V1
// $File: //depot/GeneralsMD/Staging/code/Libraries/Source/debug/debug_io_net.cpp $
// $Author: mhoffe $
// $Revision: #1 $
// $DateTime: 2003/07/03 11:55:26 $
//
// �2003 Electronic Arts
//
// Debug I/O class net (Network destination via named pipe)
//////////////////////////////////////////////////////////////////////////////
#include "_pch.h"
#include <new>      // needed for placement new prototype

#if !defined(_MSC_VER)
// Linux: named pipes become Unix-domain sockets. Same framing (type byte,
// u32 lens, payload), server listens at kDebugPipePath.
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
static const char *kDebugPipePath = "/tmp/ea_debug_v1.sock";
static inline int net_fd(HANDLE h) { return (int)(intptr_t)h; }
static inline HANDLE net_handle(int fd) { return (HANDLE)(intptr_t)fd; }
static bool net_send_all(int fd, const void *src, unsigned len)
{
  const char *p = (const char *)src;
  while (len)
  {
    ssize_t w = send(fd, p, len, MSG_NOSIGNAL);
    if (w <= 0)
      return false;
    p += w;
    len -= (unsigned)w;
  }
  return true;
}
#endif

DebugIONet::DebugIONet(void) : m_pipe(INVALID_HANDLE_VALUE)
{
}

DebugIONet::~DebugIONet()
{
  if (m_pipe!=INVALID_HANDLE_VALUE)
#if !defined(_MSC_VER)
    close(net_fd(m_pipe));
#else
    CloseHandle(m_pipe);
#endif
}

int DebugIONet::Read(char *buf, int maxchar)
{
  if (m_pipe==INVALID_HANDLE_VALUE)
    return 0;

#if !defined(_MSC_VER)
  // Non-blocking dataless poll, mirroring PIPE_NOWAIT above.
  ssize_t got = recv(net_fd(m_pipe), buf, maxchar - 1, MSG_DONTWAIT);
  return got > 0 ? (int)got : 0;
#else
  DWORD mode=PIPE_READMODE_MESSAGE|PIPE_NOWAIT;
  SetNamedPipeHandleState(m_pipe,&mode,NULL,NULL);

  DWORD read;
  if (!ReadFile(m_pipe,buf,maxchar-1,&read,NULL))
    read=0;
  mode=PIPE_READMODE_MESSAGE|PIPE_WAIT;
  SetNamedPipeHandleState(m_pipe,&mode,NULL,NULL);

  return read;
#endif
}

void DebugIONet::Write(StringType type, const char *src, const char *str)
{
  if (m_pipe==INVALID_HANDLE_VALUE)
    return;

#if !defined(_MSC_VER)
  int fd = net_fd(m_pipe);
  unsigned char t = (unsigned char)type;
  unsigned len;

  net_send_all(fd, &t, 1);
  len = src ? strlen(src) : 0;
  net_send_all(fd, &len, 4);
  if (len)
    net_send_all(fd, src, len);

  len = strlen(str);
  net_send_all(fd, &len, 4);
  if (len)
    net_send_all(fd, str, len);
#else
  DWORD dummy;
  WriteFile(m_pipe,&type,1,&dummy,NULL);

  unsigned len;
  len=src?strlen(src):0;
  WriteFile(m_pipe,&len,4,&dummy,NULL);
  if (len)
    WriteFile(m_pipe,src,len,&dummy,NULL);

  len=strlen(str);
  WriteFile(m_pipe,&len,4,&dummy,NULL);
  if (len)
    WriteFile(m_pipe,str,len,&dummy,NULL);
#endif
}

void DebugIONet::EmergencyFlush(void)
{
}

void DebugIONet::Execute(class Debug& dbg, const char *cmd, bool structuredCmd,
                         unsigned argn, const char * const * argv)
{
  if (!cmd||!strcmp(cmd,"help"))
  {
    dbg << "net I/O help:\n"
           "  add [ <machine> ]\n"
           "    create net I/O (optionally specifying the machine to connect to)\n";
  }
  else if (!strcmp(cmd,"add"))
  {
#if !defined(_MSC_VER)
    // Unix-domain socket twin of the \machine\pipe\ea_debug_v1 named pipe.
    // Only the local server exists; anything else reports the same
    // could-not-connect message as Windows without a server.
    const char *machine = argn ? argv[0] : ".";
    if (strcmp(machine, ".") && strcmp(machine, "localhost"))
    {
      dbg << "Could not connect to given machine.\n";
      return;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd >= 0)
    {
      struct sockaddr_un addr;
      memset(&addr, 0, sizeof(addr));
      addr.sun_family = AF_UNIX;
      strncpy(addr.sun_path, kDebugPipePath, sizeof(addr.sun_path) - 1);
      if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
      {
        close(fd);
        fd = -1;
      }
    }
    if (fd < 0)
    {
      dbg << "Could not connect to given machine.\n";
      return;
    }
    m_pipe = net_handle(fd);

    // write welcome message
    char comp[128];
    if (gethostname(comp, sizeof(comp) - 1) != 0)
      strcpy(comp, "localhost");
    comp[sizeof(comp) - 1] = 0;
    char buf[256];
    snprintf(buf, sizeof(buf), "Client at %s\n", comp);
    Write(Other, NULL, buf);
#else
    const char *machine=argn?argv[0]:".";

    char buf[256];
    wsprintf(buf,"\\\\%s\\pipe\\ea_debug_v1",machine);
    m_pipe=CreateFile(buf,GENERIC_READ|GENERIC_WRITE,
                      0,NULL,OPEN_EXISTING,0,NULL);
    if (m_pipe==INVALID_HANDLE_VALUE)
    {
      dbg << "Could not connect to given machine.\n";
      return;
    }

    // we're reading messages
    DWORD mode=PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(m_pipe,&mode,NULL,NULL);

    // write welcome message
    char comp[128];
    mode=sizeof(comp);
    GetComputerName(comp,&mode);
    wsprintf(buf,"Client at %s\n",comp);
    Write(Other,NULL,buf);
#endif
  }
}

DebugIOInterface *DebugIONet::Create(void)
{
  return new (DebugAllocMemory(sizeof(DebugIONet))) DebugIONet();
}

void DebugIONet::Delete(void)
{
  this->~DebugIONet();
  DebugFreeMemory(this);
}
