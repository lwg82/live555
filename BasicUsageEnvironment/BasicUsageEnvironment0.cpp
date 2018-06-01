/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2017 Live Networks, Inc.  All rights reserved.
// Basic Usage Environment: for a simple, non-scripted, console application
// Implementation

#include "BasicUsageEnvironment0.hh"
#include <stdio.h>
#if defined(__WIN32__) || defined(_WIN32) || defined(_WIN32_WCE)
#define snprintf _snprintf
#endif


////////// BasicUsageEnvironment //////////

BasicUsageEnvironment0::BasicUsageEnvironment0(TaskScheduler& taskScheduler)
  : UsageEnvironment(taskScheduler),
    fBufferMaxSize(RESULT_MSG_BUFFER_MAX) {
  reset();
}

BasicUsageEnvironment0::~BasicUsageEnvironment0() {
}

void BasicUsageEnvironment0::reset() {
  fCurBufferSize = 0;
  fResultMsgBuffer[fCurBufferSize] = '\0';
}


// Implementation of virtual functions:

char const* BasicUsageEnvironment0::getResultMsg() const {
  return fResultMsgBuffer;
}

void BasicUsageEnvironment0::setResultMsg(MsgString msg) {
  reset();
  appendToResultMsg(msg);
}

void BasicUsageEnvironment0::setResultMsg(MsgString msg1, MsgString msg2) {
  setResultMsg(msg1);
  appendToResultMsg(msg2);
}

void BasicUsageEnvironment0::setResultMsg(MsgString msg1, MsgString msg2,
				       MsgString msg3) {
  setResultMsg(msg1, msg2);
  appendToResultMsg(msg3);
}
// 先将 msg 复制到fResultMsgBuffer， 再根据 err 错误号获取错误内容，再将错误内容追加到fResultMsgBuffer
void BasicUsageEnvironment0::setResultErrMsg(MsgString msg, int err) {
  setResultMsg(msg);

  if (err == 0) err = getErrno();
#if defined(__WIN32__) || defined(_WIN32) || defined(_WIN32_WCE)
  char errMsg[RESULT_MSG_BUFFER_MAX] = "\0";
  if (0 != FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, errMsg, sizeof(errMsg)/sizeof(errMsg[0]), NULL)) {
    // Remove all trailing '\r', '\n' and '.'
    for (char* p = errMsg + strlen(errMsg); p != errMsg && (*p == '\r' || *p == '\n' || *p == '.' || *p == '\0'); --p) {
      *p = '\0';
    }
  } else
    snprintf(errMsg, sizeof(errMsg)/sizeof(errMsg[0]), "error %d", err);
  appendToResultMsg(errMsg);
#else
  appendToResultMsg(strerror(err));
#endif
}



// 将信息追加到 fResultMsgBuffer 缓存
void BasicUsageEnvironment0::appendToResultMsg(MsgString msg) {
  char* curPtr = &fResultMsgBuffer[fCurBufferSize];
  unsigned spaceAvailable = fBufferMaxSize - fCurBufferSize;
  unsigned msgLength = strlen(msg);

  // Copy only enough of "msg" as will fit:
  if (msgLength > spaceAvailable-1) {
    msgLength = spaceAvailable-1;
  }
  /*
   原型： 　void *memmove( void* dest, const void* src, size_tcount );

#include<string.h>

由src所指内存区域复制count个字节到dest所指内存区域。

src和dest所指内存区域可以重叠，但复制后dest内容会被更改。函数返回指向dest的指针。

Copies the values of num bytes from the location pointed by source to the memory block pointed by destination. Copying takes place as if an intermediate buffer were used, allowing the destination and source to overlap.

memmove的处理措施：

（1）当源内存的首地址等于目标内存的首地址时，不进行任何拷贝

（2）当源内存的首地址大于目标内存的首地址时，实行正向拷贝

（3）当源内存的首地址小于目标内存的首地址时，实行反向拷贝
  */
  memmove(curPtr, (char*)msg, msgLength);
  fCurBufferSize += msgLength;
  fResultMsgBuffer[fCurBufferSize] = '\0';
}

// 错误信息写入 stderr
void BasicUsageEnvironment0::reportBackgroundError() {
  fputs(getResultMsg(), stderr);
}

