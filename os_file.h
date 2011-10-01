#ifndef OS_FILE_H
#define OS_FILE_H
/*
   Operating system file interface.
*/


enum OSFileInfoMask
{
    FI_Size = 0x01,
    FI_Time = 0x02,
    FI_Type = 0x04
};


enum OSFileType
{
    FI_File,
    FI_Link,
    FI_Dir,
    FI_Socket,
    FI_Other
};


typedef struct
{
    int64_t size;
    double  accessed;
    double  modified;
    uint8_t type;
}
OSFileInfo;


extern int ur_fileInfo( const char* path, OSFileInfo* info, int mask );


#endif  /* OS_FILE_H */
