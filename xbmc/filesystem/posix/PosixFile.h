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

#pragma once

#include "filesystem/IFile.h"

namespace XFILE
{
class CPosixFile : public IFile
{
public:
  CPosixFile();
  virtual ~CPosixFile();
  virtual int64_t GetPosition();
  virtual int64_t GetLength();
  virtual bool Open(const CURL& url);
  virtual bool Exists(const CURL& url);
  virtual int Stat(const CURL& url, struct __stat64* buffer);
  virtual int Stat(struct __stat64* buffer);
  virtual unsigned int Read(void* lpBuf, int64_t uiBufSize);
  virtual int Write(const void* lpBuf, int64_t uiBufSize);
  virtual int64_t Seek(int64_t iFilePosition, int iWhence = SEEK_SET);
  virtual int Truncate(int64_t size);
  virtual void Close();
  virtual void Flush();

  virtual bool OpenForWrite(const CURL& url, bool bOverWrite = false);

  virtual bool Delete(const CURL& url);
  virtual bool Rename(const CURL& url, const CURL& urlnew);
  virtual bool SetHidden(const CURL& url, bool hidden);

  virtual int IoControl(EIoControl request, void* param);
protected:
  std::string GetLocal(const CURL &url); /* crate a properly format path from an url */
  FILE* m_file;
  int64_t m_i64FilePos;
  int64_t m_i64FileLen;
  int64_t m_i64LastDropPos;
};

}
