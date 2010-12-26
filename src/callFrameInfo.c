/*
  File: callFrameInfo.c
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: Katana is free software: you may redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation, either version 2 of the
  License, or (at your option) any later version. Regardless of
  which version is chose, the following stipulation also applies:
    
  Any redistribution must include copyright notice attribution to
  Dartmouth College as well as the Warranty Disclaimer below, as well as
  this list of conditions in any related documentation and, if feasible,
  on the redistributed software; Any redistribution must include the
  acknowledgment, “This product includes software developed by Dartmouth
  College,” in any related documentation and, if feasible, in the
  redistributed software; and The names “Dartmouth” and “Dartmouth
  College” may not be used to endorse or promote products derived from
  this software.  

  WARRANTY DISCLAIMER

  PLEASE BE ADVISED THAT THERE IS NO WARRANTY PROVIDED WITH THIS
  SOFTWARE, TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT WHEN
  OTHERWISE STATED IN WRITING, DARTMOUTH COLLEGE, ANY OTHER COPYRIGHT
  HOLDERS, AND/OR OTHER PARTIES PROVIDING OR DISTRIBUTING THE SOFTWARE,
  DO SO ON AN "AS IS" BASIS, WITHOUT WARRANTY OF ANY KIND, EITHER
  EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE
  SOFTWARE FALLS UPON THE USER OF THE SOFTWARE. SHOULD THE SOFTWARE
  PROVE DEFECTIVE, YOU (AS THE USER OR REDISTRIBUTOR) ASSUME ALL COSTS
  OF ALL NECESSARY SERVICING, REPAIR OR CORRECTIONS.

  IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
  WILL DARTMOUTH COLLEGE OR ANY OTHER COPYRIGHT HOLDER, OR ANY OTHER
  PARTY WHO MAY MODIFY AND/OR REDISTRIBUTE THE SOFTWARE AS PERMITTED
  ABOVE, BE LIABLE TO YOU FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL,
  INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR
  INABILITY TO USE THE SOFTWARE (INCLUDING BUT NOT LIMITED TO LOSS OF
  DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR
  THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER
  PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGES.

  The complete text of the license may be found in the file COPYING
  which should have been distributed with this software. The GNU
  General Public License may be obtained at
  http://www.gnu.org/licenses/gpl.html

  Project: Katana
  Date: December 2010
  Description: Data structures and methods dealing with call frame information
*/

#include "callFrameInfo.h"
#include "constants.h"
#include "leb.h"

typedef struct
{
  byte* data;
  int len;
  int allocated;
} GrowingBuffer;

static void addToGrowingBuffer(GrowingBuffer* buf,void* data,int dataLen)
{
  if(buf->len+dataLen > buf->allocated)
  {
    buf->allocated=(buf->len+dataLen)*2;
    buf->data=realloc(buf->data,buf->allocated);
  }
  memcpy(buf->data+buf->len,data,dataLen);
  buf->len+=dataLen;
}

void buildCIERawData(CIE* cie,GrowingBuffer* buf,bool isEHFrame)
{
  //todo: support 64-bit DWARF format. Here I am always building 32-bit dwarf format

  //part one of the header, before the augmentation string
  struct
  {
    uint length;
    uint CIE_id;
    byte version;
  } __attribute__((__packed__)) header1;

  //part2 of the header, after the augmentation string
  //only present in Dwarf v4
  struct
  {
    byte address_size;
    byte segment_size;
  } __attribute__((__packed__)) header2;

  //we'll set the length last as we don't know it yet
  header1.CIE_id=isEHFrame?EH_CIE_ID:DEBUG_CIE_ID;
  header1.version=cie->version;
  assert(cie->addressSize<256);
  assert(cie->segmentSize<256);
  header2.address_size=cie->addressSize;
  header2.segment_size=cie->segmentSize;

  //compute the LEB values we need: code-align, data-align, and return-address-rule
  usint codeAlignLen,dataAlignLen,returnAddrRegLen;
  byte* codeAlign=uintToLEB128(cie->codeAlign,&codeAlignLen);
  byte* dataAlign=intToLEB128(cie->dataAlign,&dataAlignLen);
  byte* returnAddrReg=uintToLEB128(cie->returnAddrRuleNum,&returnAddrRegLen);

  byte* augmentationDataLength;
  usint augmentationDataLengthLen=0;
  //from the Linux Standard's Base
  //http://refspecs.freestandards.org/LSB_3.0.0/LSB-Core-generic/LSB-Core-generic/ehframechpt.html
  if(isEHFrame && strstr(cie->augmentation,"z"))
  {
    augmentationDataLength=uintToLEB128(cie->augmentationDataLen,&augmentationDataLengthLen);
  }
  

  DwarfInstructions rawInstructions=
    serializeDwarfRegInstructions(cie->initialInstructions,
                                  cie->numInitialInstructions);

  header1.length=sizeof(header1)-sizeof(header1.length)+strlen(cie->augmentation)+1+codeAlignLen+dataAlignLen+returnAddrRegLen+cie->augmentationDataLen+augmentationDataLengthLen+rawInstructions.numBytes;
  if(cie->version >= 4)
  {
    header1.length+=sizeof(header2);
  }
  //CIE structures are required to be aligned
  int paddingNeeded=0;
  if(header1.length % cie->addressSize)
  {
    paddingNeeded=cie->addressSize - (header1.length % cie->addressSize);
    header1.length+=paddingNeeded;
  }

  byte* padding=malloc(paddingNeeded);
  MALLOC_CHECK(padding);
  memset(padding,DW_CFA_nop,paddingNeeded);

  //now we can actually write everythingo out
  addToGrowingBuffer(buf,&header1,sizeof(header1));
  addToGrowingBuffer(buf,cie->augmentation,strlen(cie->augmentation)+1);
  if(cie->version >= 4)
  {
    addToGrowingBuffer(buf,&header2,sizeof(header2));
  }
  addToGrowingBuffer(buf,codeAlign,codeAlignLen);
  addToGrowingBuffer(buf,dataAlign,dataAlignLen);
  addToGrowingBuffer(buf,returnAddrReg,returnAddrRegLen);
  if(augmentationDataLengthLen)
  {
    addToGrowingBuffer(buf,augmentationDataLength,augmentationDataLengthLen);
  }
  if(cie->augmentationDataLen)
  {
    addToGrowingBuffer(buf,cie->augmentationData,cie->augmentationDataLen);
  }
  addToGrowingBuffer(buf,rawInstructions.instrs,rawInstructions.numBytes);
  addToGrowingBuffer(buf,padding,paddingNeeded);
  destroyRawInstructions(rawInstructions);
}

void buildFDERawData(FDE* fde,uint* cieOffsets,GrowingBuffer* buf)
{
  //todo: support 64-bit DWARF format. Here I am always building 32-bit dwarf format

  //part one of the header, before the augmentation string
  struct
  {
    uint length;
    uint CIE_offset;
    addr_t initial_location;
    addr_t address_range;
  } __attribute__((__packed__)) header;


  //if this isn't true our structure won't be the right size
  //note that we aren't able to deal with DWARF stuff cross-architecture
  //this should work on both 64 and 32 bit but the architecture of the machine
  //must match the ELF/DWARF being examined
  assert(fde->cie);
  assert(fde->cie->addressSize==sizeof(addr_t));

  DwarfInstructions rawInstructions=
    serializeDwarfRegInstructions(fde->instructions,
                                  fde->numInstructions);

  header.length=sizeof(header)-sizeof(header.length)+rawInstructions.numBytes;

  int paddingNeeded=0;
  byte* padding=NULL;
  if(header.length % fde->cie->addressSize)
  {
    paddingNeeded=fde->cie->addressSize - (header.length % fde->cie->addressSize);
    header.length+=paddingNeeded;
    padding=malloc(paddingNeeded);
    MALLOC_CHECK(padding);
    memset(padding,DW_CFA_nop,paddingNeeded);
  }

  header.CIE_offset=cieOffsets[fde->cie->idx];
  header.initial_location=fde->lowpc;
  header.address_range=fde->highpc-fde->lowpc;


  //now we can actually write everythingo out
  addToGrowingBuffer(buf,&header,sizeof(header));
  addToGrowingBuffer(buf,rawInstructions.instrs,rawInstructions.numBytes);
  if(paddingNeeded)
  {
    addToGrowingBuffer(buf,padding,paddingNeeded);
  }
  destroyRawInstructions(rawInstructions);
}

//returns a void* to a binary representation of a Dwarf call frame
//information section (i.e. .debug_frame in the dwarf specification)
//the length of the returned buffer is written into byteLen.
//the memory for the buffer should free'd when the caller is finished with it
byte* buildCallFrameSectionData(CallFrameInfo* cfi,int* byteLen)
{
  GrowingBuffer buf;
  //allocate 1k initially as an arbitrary amount. We'll grow it as needed
  buf.allocated=10000;
  buf.len=0;
  buf.data=malloc(buf.allocated);
  uint* cieOffsets=zmalloc(cfi->numCIEs*sizeof(uint));
  for(int i=0;i<cfi->numCIEs;i++)
  {
    cieOffsets[i]=buf.len;
    buildCIERawData(cfi->cies+i,&buf,cfi->isEHFrame);
  }
  for(int i=0;i<cfi->numFDEs;i++)
  {
    buildFDERawData(cfi->fdes+i,cieOffsets,&buf);
  }
  *byteLen=buf.len;
  return buf.data;
}
