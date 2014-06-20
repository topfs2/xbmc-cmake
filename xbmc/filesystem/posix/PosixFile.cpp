/*
 *      Copyright (C) 2014 Arne Morten Kvarving
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "system.h"
#include "PosixFile.h"
#include "URL.h"
#include "utils/AliasShortcutUtils.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "PlatformDefs.h"

using namespace XFILE;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

//*********************************************************************************************
CPosixFile::CPosixFile()
    : m_file(NULL),
      m_i64LastDropPos(0)
{}

//*********************************************************************************************
CPosixFile::~CPosixFile()
{
  if (m_file)
    fclose(m_file);
}
//*********************************************************************************************
std::string CPosixFile::GetLocal(const CURL &url)
{
  std::string path(url.GetFileName());

  if(url.GetProtocol() == "file")
  {
    // file://drive[:]/path
    // file:///drive:/path
    std::string host(url.GetHostName());

    if(!host.empty())
    {
      if(host[host.length()-1] == ':')
        path = host + "/" + path;
      else
        path = host + ":/" + path;
    }
  }

  if (IsAliasShortcut(path))
    TranslateAliasShortcut(path);

  return path;
}

//*********************************************************************************************
bool CPosixFile::Open(const CURL& url)
{
  std::string strFile(GetLocal(url));

  m_file = fopen(strFile.c_str(), "r");

  m_i64FilePos = 0;
  m_i64FileLen = 0;
  m_i64LastDropPos = 0;

  return m_file != NULL;
}

bool CPosixFile::Exists(const CURL& url)
{
  std::string strFile(GetLocal(url));

  struct __stat64 buffer;
  return (stat64(strFile.c_str(), &buffer)==0);
}

int CPosixFile::Stat(struct __stat64* buffer)
{
  return fstat64(fileno(m_file), buffer);
}

int CPosixFile::Stat(const CURL& url, struct __stat64* buffer)
{
  std::string strFile(GetLocal(url));

  return stat64(strFile.c_str(), buffer);
}

bool CPosixFile::SetHidden(const CURL &url, bool hidden)
{
  return false;
}

//*********************************************************************************************
bool CPosixFile::OpenForWrite(const CURL& url, bool bOverWrite)
{
  // make sure it's a legal FATX filename (we are writing to the harddisk)
  std::string strPath(GetLocal(url));

  m_file = fopen(strPath.c_str(), bOverWrite?"w+":"a+");
  if (!m_file)
    return false;

  m_i64FilePos = 0;
  Seek(0, SEEK_SET);

  return true;
}

//*********************************************************************************************
unsigned int CPosixFile::Read(void *lpBuf, int64_t uiBufSize)
{
  if (!m_file)
    return 0;
  size_t nBytesRead = fread(lpBuf, 1, uiBufSize, m_file);
  if (nBytesRead)
  {
    m_i64FilePos += nBytesRead;
#if defined(HAVE_POSIX_FADVISE)
    // Drop the cache between where we last seeked and 16 MB behind where
    // we are now, to make sure the file doesn't displace everything else.
    // However, we never throw out the first 16 MB of the file, as we might
    // want the header etc., and we never ask the OS to drop in chunks of
    // less than 1 MB.
    int64_t start_drop = std::max<int64_t>(m_i64LastDropPos, 16 << 20);
    int64_t end_drop = std::max<int64_t>(m_i64FilePos - (16 << 20), 0);
    if (end_drop - start_drop >= (1 << 20))
    {
      posix_fadvise(fileno(m_file), start_drop, end_drop - start_drop, POSIX_FADV_DONTNEED);
      m_i64LastDropPos = end_drop;
    }
#endif
    return nBytesRead;
  }
  return 0;
}

//*********************************************************************************************
int CPosixFile::Write(const void *lpBuf, int64_t uiBufSize)
{
  if (!m_file)
    return 0;

  size_t nBytesWritten = fwrite(lpBuf, 1, uiBufSize, m_file);
  return nBytesWritten;
}

//*********************************************************************************************
void CPosixFile::Close()
{
  fclose(m_file);
  m_file = NULL;
}

//*********************************************************************************************
int64_t CPosixFile::Seek(int64_t iFilePosition, int iWhence)
{
  if (fseek(m_file, iFilePosition, iWhence))
    return -1;
  int64_t newpos = ftell(m_file);

  if (m_i64FilePos != newpos)
  {
    // If we seek, disable the cache drop heuristic until we
    // have played sequentially for a while again from here.
    m_i64LastDropPos = newpos;
  }
  m_i64FilePos = newpos;

  return m_i64FilePos;
}

//*********************************************************************************************
int64_t CPosixFile::GetLength()
{
  int64_t pos = m_i64FilePos;
  fseek(m_file, 0, SEEK_END);
  int64_t res = ftell(m_file);
  fseek(m_file, pos, SEEK_SET);
  return res;
}

//*********************************************************************************************
int64_t CPosixFile::GetPosition()
{
  return m_i64FilePos;
}

bool CPosixFile::Delete(const CURL& url)
{
  std::string strFile(GetLocal(url));
  return unlink(strFile.c_str()) == 0;
}

bool CPosixFile::Rename(const CURL& url, const CURL& urlnew)
{
  std::string strFile(GetLocal(url));
  std::string strNewFile(GetLocal(urlnew));

  return rename(strFile.c_str(), strNewFile.c_str()) == 0;
}

void CPosixFile::Flush()
{
  fsync(fileno(m_file));
}

int CPosixFile::IoControl(EIoControl request, void* param)
{
  if(request == IOCTRL_NATIVE && param)
  {
    SNativeIoControl* s = (SNativeIoControl*)param;
    return ioctl(fileno(m_file), s->request, s->param);
  }
  return -1;
}

int CPosixFile::Truncate(int64_t size)
{
  return ftruncate(fileno(m_file), (off_t) size);
}
