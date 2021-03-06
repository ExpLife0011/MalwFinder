

#include "MalwFind_DataTypes.h"
#include "MalwFind.h"
#include "MalwFind_FileFunc.h"
#include "MalwFind_Util.h"


extern POBJECT_TYPE NTSYSAPI IoDriverObjectType;


extern const WCHAR* g_pEXE_MALWFIND_AGENT;

extern G_MALW_FIND  g_MalwFind;


WCHAR  
ConvertUpper(WCHAR wChar)
{
	if(wChar >= L'a' && wChar <= L'z')
	{
		wChar -= 32;
	}
	return wChar;
}


BOOLEAN  
Search_WildcardToken(const WCHAR* pwzBuffer, const WCHAR* pwzWildcardToken)
{
	__try
	{
		while(*pwzWildcardToken)
		{
			if(*pwzWildcardToken == L'?')
			{
				if(!*pwzBuffer) return FALSE;

				++pwzBuffer;
				++pwzWildcardToken;
			}
			else if(*pwzWildcardToken == L'*')
			{
				if(Search_WildcardToken( pwzBuffer, pwzWildcardToken+1 )) 
				{
					return TRUE;
				}
				if(*pwzBuffer && Search_WildcardToken( pwzBuffer+1, pwzWildcardToken )) 
				{
					return TRUE;
				}

				return FALSE;
			}
			else
			{
				if(ConvertUpper( *pwzBuffer++ ) != ConvertUpper( *pwzWildcardToken++ ))
				{		
					return FALSE;
				}
			}
		}

	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		return FALSE;
	}

	return !(*pwzBuffer) && !(*pwzWildcardToken);

}


// pLogDiskDeviceObject
// pLowerDeviceObject
NTSTATUS  
Config_SetDeviceObject( IN PFLT_DRV_CONFIG pConfig )
{
	NTSTATUS        Status            = STATUS_SUCCESS;
	PFILE_OBJECT    pFileObject       = NULL;
	PDEVICE_OBJECT  pFindDeviceObject = NULL;
	PDEVICE_OBJECT  pDeviceObject     = NULL;
	PDEVICE_OBJECT  pDiskDeviceObject = NULL;
	PVPB            pVpb              = NULL;
	WCHAR           ShortBuffer[ MAX_VOLUME_NAME ] = L"";
	WCHAR           DeviceBuffer[ MAX_PATH ] = L"";
	UNICODE_STRING  DeviceName = {0};

	if(KeGetCurrentIrql() >= DISPATCH_LEVEL) 
	{
		return STATUS_UNSUCCESSFUL;
	}
		
	if(!pConfig || !pConfig->ulSystemRoot_Length)
	{
		ASSERT( pConfig && pConfig->ulSystemRoot_Length );
		KdPrint(("STATUS_INVALID_PARAMETER >> Config_SetDeviceObject 1. \n"));
		return STATUS_INVALID_PARAMETER;
	}

	RtlZeroMemory( DeviceBuffer, MAX_PATH*sizeof(WCHAR) );
	RtlZeroMemory( ShortBuffer,  MAX_VOLUME_NAME*sizeof(WCHAR) );	
	ShortBuffer[0] = pConfig->wzSystemRoot[0];
	ShortBuffer[1] = L':';
	ShortBuffer[2] = L'\0';
	
	RtlStringCchCopyW( DeviceBuffer, MAX_PATH, FSD_DOSDEVICES );
	RtlStringCchCatW(  DeviceBuffer, MAX_PATH, ShortBuffer ); 

	RtlInitUnicodeString( &DeviceName, DeviceBuffer );
	Status = IoGetDeviceObjectPointer( &DeviceName, FILE_ANY_ACCESS, &pFileObject, &pDeviceObject  ); 
	if(!NT_SUCCESS( Status ))
	{
		return Status;
	}
	
	// LogDisk Object   FileObject 에 세팅할것임
	if(pFileObject)
	{
		pConfig->pLogDiskDeviceObject = pDiskDeviceObject = pFileObject->DeviceObject;
		if(!pDiskDeviceObject) 
		{
			if(pFileObject) ObDereferenceObject( pFileObject );
			return STATUS_NO_SUCH_DEVICE;
		}
	}

	if(pFileObject) 
	{
		ObDereferenceObject( pFileObject );
	}

	// pLowerDeviceObject
	// pDiskDeviceObject  대부분  FtDisk.sys 의 DeviceObject 일것이다. 고로 Mount 되어 있어야 한다.
	pVpb = pDiskDeviceObject->Vpb;
	if(!pVpb || !pVpb->DeviceObject)
	{
		return STATUS_NO_SUCH_DEVICE;
	}

	pFindDeviceObject = pVpb->DeviceObject;
	if(!pFindDeviceObject)
	{
		return Status;
	}

	while(pFindDeviceObject->AttachedDevice)
	{
		if(IS_MALFIND_DEVICE_OBJECT( pFindDeviceObject->AttachedDevice ))
		{
			pConfig->pLowerDeviceObject = pFindDeviceObject;
			break;
		}
		pFindDeviceObject = pFindDeviceObject->AttachedDevice;
	}
	return Status;

}


// SystemRoot Get
PWCHAR Config_GetSystemRoot(void)
{
	return g_MalwFind.DrvConfig.wzSystemRoot;
}


// SetupDir Get
PWCHAR Config_GetSetupDir(void)
{
	return g_MalwFind.DrvConfig.wzSetupDir;
}




// 프로세스명
/*
BOOLEAN
GetProcessName( IN PEPROCESS   pProcess, 
				IN OUT WCHAR*  pwzOutProcName, 
				IN ULONG       ulMaxProcName )
{
	ULONG        ulReturn     = 0;
	PWCHAR       pwzProcName  = NULL;
	char*        pProcessName = NULL;
	char         czProcName[ MAX_PROCESS_LEN ] = "";
	WCHAR        wzProcName[ MAX_PROCESS_LEN ] = L"";
	NAME_BUFFER  FullPath = {0};
	HANDLE       hProcessId   = NULL;

	if(!pProcess || !pwzOutProcName ) return FALSE;
	if(KeGetCurrentIrql() >= DISPATCH_LEVEL) return FALSE;

	FsRtlEnterFileSystem();

	__try
	{
		pProcessName = (char*)pProcess + g_MalwFind.RefProc.ulImageName;
		ASSERT( pProcessName );
		if(!pProcessName) 
		{
			FsRtlExitFileSystem();
			return FALSE;
		}

		if(MmIsAddressValid( wzProcName ) && MmIsAddressValid( czProcName ))
		{
			RtlStringCchCopyA( czProcName, MAX_PROCESS_LEN, pProcessName );		
			RtlMultiByteToUnicodeN( wzProcName, MAX_PROCESS_LEN, &ulReturn, czProcName, strlen(czProcName) );
			RtlStringCchCopyW( pwzOutProcName, ulMaxProcName, wzProcName );		
		}

	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		KdPrint(("GetProcessName : Exception Occured! \n"));
		FsRtlExitFileSystem();
		return FALSE;
	}

	FsRtlExitFileSystem();
	return TRUE;

}
*/



// 프로세스명
BOOLEAN GetProcessName(IN PEPROCESS pProcess, IN OUT WCHAR* pwzOutProcName, IN ULONG ulMaxProcName)
{
	ULONG        ulReturn     = 0;
	PWCHAR       pwzProcName  = NULL;
	char*        pProcessName = NULL;
	char         czProcName[ MAX_PROCESS_LEN ] = "";
	WCHAR        wzProcName[ MAX_PROCESS_LEN ] = L"";
	NAME_BUFFER  FullPath = {0};
	HANDLE       hProcessId   = NULL;
	
	if(!pProcess || !pwzOutProcName ) return FALSE;
	if(KeGetCurrentIrql() >= DISPATCH_LEVEL)
	{
		return FALSE;
	}

	hProcessId = PsGetCurrentProcessId();
	if(g_MalwFind.ulOSVer >= OS_VER_WXP && ExGetPreviousMode() != KernelMode && KeGetCurrentIrql() == PASSIVE_LEVEL)
	{		
		ALLOCATE_N_POOL( FullPath );
		if(FullPath.pBuffer)
		{
			SET_POOL_ZERO( FullPath );
			ulReturn = GetProcessFullNameEx( hProcessId, &FullPath );
			if(ulReturn >= 2)
			{			
				pwzProcName = wcsrchr( FullPath.pBuffer, L'\\' );
				if(pwzProcName) pwzProcName++;
				if(pwzProcName)
				{
					RtlStringCchCopyW( pwzOutProcName, ulMaxProcName, pwzProcName );		
				}
				if(FullPath.pBuffer) FREE_N_POOL( FullPath );
				return TRUE;
			}			
			if(FullPath.pBuffer) FREE_N_POOL( FullPath );
		}
	}


	__try
	{
		KeEnterCriticalRegion();
		ExAcquireResourceExclusiveLite( &g_MalwFind.LockResource, TRUE );

		pProcessName = (char*)pProcess + g_MalwFind.RefProc.ulImageName;
		if(!pProcessName) 
		{
			ExReleaseResourceLite( &g_MalwFind.LockResource );
			KeLeaveCriticalRegion();
			return FALSE;
		}
		if(MmIsAddressValid( wzProcName ) && MmIsAddressValid( czProcName ))
		{
			RtlStringCchCopyA( czProcName, MAX_PROCESS_LEN, pProcessName );			
			mbstowcs( wzProcName, czProcName, MAX_PROCESS_LEN );
			RtlStringCchCopyW( pwzOutProcName, ulMaxProcName, wzProcName );		
		}

		ExReleaseResourceLite( &g_MalwFind.LockResource );
		KeLeaveCriticalRegion();

	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		KdPrint(("GetProcessName : Exception Occured! PID=%04d \n", hProcessId ));
		ExReleaseResourceLite( &g_MalwFind.LockResource );
		KeLeaveCriticalRegion();
		return FALSE;
	}

	return TRUE;

}




ULONG GetProcessFullNameEx( IN HANDLE hPid, IN OUT PNAME_BUFFER pRefProc )
{
	NTSTATUS           Status = STATUS_SUCCESS;
	OBJECT_ATTRIBUTES  ObjectAttr;
	CLIENT_ID          ClientID;
	HANDLE             hProc = NULL;
	ULONG              ulRet = 0;
	PUNICODE_STRING    pProcName = NULL;
			
	if(!pRefProc || !pRefProc->pBuffer) return 0;
	if(ExGetPreviousMode() == KernelMode) return 0;
	if(KeGetCurrentIrql() > PASSIVE_LEVEL) 
	{
		KdPrint(("GetProcessFullName_Ex >> IRQL > PASSIVE_LEVEL. ProcID=%04d \n", hPid ));
		return 0;
	}
		
	__try
	{
		ClientID.UniqueThread  = NULL;
		ClientID.UniqueProcess = hPid;		
		InitializeObjectAttributes( &ObjectAttr, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );
		Status = ZwOpenProcess( &hProc, 0x0010 | 0x0400, &ObjectAttr, &ClientID);
		if(!NT_SUCCESS( Status ) || !hProc)
		{
			return 0;
		}

		KeEnterCriticalRegion();
		ExAcquireResourceExclusiveLite( &g_MalwFind.LockResource, TRUE );
	
		Status = ZwQueryInformationProcess( (HANDLE)hProc, ProcessImageFileName, pRefProc->pBuffer, pRefProc->ulMaxLength, NULL );
		if(!NT_SUCCESS( Status ))
		{
			if(hProc) ZwClose( hProc );
			ExReleaseResourceLite( &g_MalwFind.LockResource );
			KeLeaveCriticalRegion();
			return 0;
		}

		pProcName = (PUNICODE_STRING)pRefProc->pBuffer;
		if(pProcName)
		{						
			pRefProc->ulLength = ulRet = pProcName->Length;
			RtlStringCchCopyW( pRefProc->pBuffer, pRefProc->ulMaxLength, pProcName->Buffer );
		}

		if(hProc) ZwClose( hProc );
		ExReleaseResourceLite( &g_MalwFind.LockResource );
		KeLeaveCriticalRegion();

	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{			
		KdPrint(("GetProcessFullName_Ex Exception Occured. ProcID=%04d \n", hPid ));
		
		if(hProc) ZwClose( hProc );
		ExReleaseResourceLite( &g_MalwFind.LockResource );
		KeLeaveCriticalRegion();
		return 0;
	}
	
	return ulRet;

}



NTSTATUS
GetDosNameToLinkTarget( IN PWCHAR pzSrcNameStr, IN OUT PWCHAR pzOutTarget, ULONG ulMaxTarget )
{
    static WCHAR        TagetNameBuffer[50];
    UNICODE_STRING      TargetName;
	UNICODE_STRING      SrcName;

    HANDLE              hLink =  NULL;
    HANDLE              hDir  =  NULL;
	OBJECT_ATTRIBUTES   ObjectAttr;
	NTSTATUS            Status = STATUS_UNSUCCESSFUL;

	if(!pzSrcNameStr || !pzOutTarget || !ulMaxTarget) 
	{
		KdPrint(("STATUS_INVALID_PARAMETER >> GetDosNameToLinkTarget 1. \n"));
		return STATUS_INVALID_PARAMETER;
	}

    RtlInitUnicodeString( &SrcName, (PWCHAR)L"\\??" );    
	InitializeObjectAttributes( &ObjectAttr, &SrcName, OBJ_KERNEL_HANDLE, (HANDLE)NULL, (PSECURITY_DESCRIPTOR)NULL );	

	Status = ZwOpenDirectoryObject( &hDir, DIRECTORY_QUERY, &ObjectAttr );
    if(!NT_SUCCESS( Status )) return Status;

    // Open symbolic link object(s)    
	RtlInitUnicodeString( &SrcName, pzSrcNameStr );
    InitializeObjectAttributes( &ObjectAttr, &SrcName, OBJ_KERNEL_HANDLE, (HANDLE)hDir, (PSECURITY_DESCRIPTOR)NULL );

    Status = ZwOpenSymbolicLinkObject( &hLink, SYMBOLIC_LINK_QUERY, &ObjectAttr );
	if(!NT_SUCCESS( Status ))
	{
		ZwClose( hDir );  
		hDir = NULL;
		return Status;
	}

	RtlZeroMemory( TagetNameBuffer, sizeof(TagetNameBuffer) );
	RtlInitUnicodeString( &TargetName, TagetNameBuffer );

	Status = ZwQuerySymbolicLinkObject( hLink, &TargetName, NULL );
	if(NT_SUCCESS( Status ))
	{
		RtlStringCchCopyW( pzOutTarget, ulMaxTarget, TargetName.Buffer );
	}

	ZwClose(hLink);   hLink = NULL; 
    ZwClose( hDir );  hDir = NULL;

    return Status;

}   




NTSTATUS
GetDosNameToLinkTarget_Ex( IN PWCHAR pzSrcNameStr, IN OUT PWCHAR pzOutTarget, ULONG ulMaxTarget )
{
    static WCHAR        TagetNameBuffer[50];
    UNICODE_STRING      TargetName;
	UNICODE_STRING      SrcName;

    HANDLE              hLink =  NULL;
    HANDLE              hDir  =  NULL;
	OBJECT_ATTRIBUTES   ObjectAttr;
	NTSTATUS            Status = STATUS_UNSUCCESSFUL;

	if(!pzSrcNameStr || !pzOutTarget || !ulMaxTarget) 
	{
		KdPrint(("STATUS_INVALID_PARAMETER >> GetDosNameToLinkTarget_Ex 1. \n"));
		return STATUS_INVALID_PARAMETER;
	}

    RtlInitUnicodeString( &SrcName, (PWCHAR)L"\\GLOBAL??" );    
	InitializeObjectAttributes( &ObjectAttr, &SrcName, OBJ_KERNEL_HANDLE, (HANDLE)NULL, (PSECURITY_DESCRIPTOR)NULL );
	
	Status = ZwOpenDirectoryObject( &hDir, DIRECTORY_QUERY, &ObjectAttr );
    if(!NT_SUCCESS( Status ))  return Status;

    // Open symbolic link object(s)    
	RtlInitUnicodeString( &SrcName, pzSrcNameStr );
    InitializeObjectAttributes( &ObjectAttr, &SrcName, OBJ_KERNEL_HANDLE, (HANDLE)hDir, (PSECURITY_DESCRIPTOR)NULL );

    Status = ZwOpenSymbolicLinkObject( &hLink, SYMBOLIC_LINK_QUERY, &ObjectAttr );
	if(!NT_SUCCESS( Status ))
	{
		ZwClose( hDir );  
		hDir = NULL;
		return Status;
	}

	RtlZeroMemory( TagetNameBuffer, sizeof(TagetNameBuffer) );
	RtlInitUnicodeString( &TargetName, TagetNameBuffer );

	Status = ZwQuerySymbolicLinkObject( hLink, &TargetName, NULL );
	if(NT_SUCCESS( Status ))
	{
		RtlStringCchCopyW( pzOutTarget, ulMaxTarget, TargetName.Buffer );
	}

	ZwClose(hLink);   hLink = NULL; 
    ZwClose( hDir );  hDir = NULL;

    return Status;
}   






// Written by taehwauser 
// 드라이버의 이름을 가지고 드라이버 오브젝트의 포인터를 찾는함수 
PDRIVER_OBJECT  
FindDriverObject( PUNICODE_STRING pUnicodeName )
{
	NTSTATUS          Status           = STATUS_SUCCESS;
    HANDLE            hDrvHandle       = NULL;
    PDRIVER_OBJECT     pDriverObject    = NULL;
	OBJECT_ATTRIBUTES  ObjectAttributes = {0};

	__try
	{

		InitializeObjectAttributes( &ObjectAttributes, pUnicodeName, OBJ_CASE_INSENSITIVE, NULL, NULL );

		// 이름을 가지고 핸들을 구한다.
		Status = ObOpenObjectByName( &ObjectAttributes, IoDriverObjectType, KernelMode, 0L, FILE_READ_DATA, NULL, &hDrvHandle );
		if(!NT_SUCCESS( Status )) return (PDRIVER_OBJECT)NULL;

		// 오브젝트 메니저에서 드라이버 오브젝트의 포인터를 구해서 온다.
		Status = ObReferenceObjectByHandle( hDrvHandle, GENERIC_READ, NULL, 0, &pDriverObject, NULL );
		if(!NT_SUCCESS( Status ))
		{
			ZwClose( hDrvHandle );
			return (PDRIVER_OBJECT)NULL;
		}

	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		if(hDrvHandle)    ZwClose( hDrvHandle );
		if(pDriverObject) ObDereferenceObject( pDriverObject );
		return pDriverObject;
	}

    ZwClose( hDrvHandle );
    ObDereferenceObject( pDriverObject );
    return pDriverObject;

}


// Written by taehwauser 
// Floppy A: B: 드라이브가 마운트 되어있는지 검사 하는 함수
// RETURN : TRUE   -- Mount 되어있다.
//          FALSE  -- Mount 되어있지 않다.
BOOLEAN  
FloppyCheckMounted( IN PWCHAR pDosName )
{
	PVPB  pVpb = NULL;
	NTSTATUS Status = 0;
	PDRIVER_OBJECT pDriverObject = NULL;
	PDEVICE_OBJECT pDevice = NULL;

	UNICODE_STRING  DosDevicesName, DriverName, LetterName;

	ASSERT( pDosName );
	if(!pDosName) return FALSE;
	
	// "\\DosDevices\\A:"  "\\DosDevices\\B:"
	RtlInitUnicodeString( &DosDevicesName, pDosName + wcslen(FSD_DOSDEVICES) ); 
	//  L"\\Driver\\FlpyDisk" 
	RtlInitUnicodeString( &DriverName, DRIVER_FLPY );

	pDriverObject = FindDriverObject( &DriverName );
	if(!pDriverObject)  return FALSE;
	// CDO or VDO
	pDevice = pDriverObject->DeviceObject;
	if(!pDevice) return FALSE;

	while(pDevice)
	{
		pVpb = pDevice->Vpb;		
		if(pVpb && pVpb->Flags == VPB_MOUNTED)  // 마운트 플래그가 설정되어 있는지 여부를 판단한다.
		{
			LetterName.Buffer = NULL;
			if(ExGetPreviousMode() != KernelMode) 
			{
				if(g_MalwFind.ulOSVer >= OS_VER_WXP)
				{
					Status = IoVolumeDeviceToDosName( pDevice, &LetterName);
				}
				else
				{
					Status = RtlVolumeDeviceToDosName( pDevice, &LetterName);
				}

				// VDO의 볼륨명을 비교한다. 마운트 되어 있을때만 유효하다. 
				if(RtlCompareUnicodeString( &DosDevicesName, &LetterName, TRUE ) == 0)
				{
					ExFreePool( LetterName.Buffer );
					LetterName.Buffer = NULL;
					return TRUE;
				}
				if(LetterName.Buffer)
				{
					ExFreePool( LetterName.Buffer );
					LetterName.Buffer = NULL;
				}
			}

		}
		pDevice = pDevice->NextDevice;  // 드라이버오브젝트가 이전에 생성한 디바이스 오브젝트를 찾는다. // 물리디바이스나 가상디바이스를 나타낸다.
	}

	return FALSE;
}
 


// FSD_DRIVER_DISK
/*
BOOLEAN  
f( IN PWCHAR pRealDriverName, IN PWCHAR pDosName )
{
	PVPB  pVpb = NULL;
	NTSTATUS Status = 0;
	PDRIVER_OBJECT pDriverObject = NULL;
	PDEVICE_OBJECT pDevice = NULL;
	UNICODE_STRING  DosDevicesName, DriverName, LetterName;

	ASSERT( pRealDriverName && pDosName );
	if(!pRealDriverName || !pDosName) return FALSE;
	
	// "\\DosDevices\\F:"  "\\DosDevices\\B:"
	RtlInitUnicodeString( &DosDevicesName, pDosName + wcslen(FSD_DOSDEVICES) ); 
	//  L"\\Driver\\FlpyDisk" 
	RtlInitUnicodeString( &DriverName, pRealDriverName );

	pDriverObject = FindDriverObject( &DriverName );
	if(!pDriverObject)  return FALSE;
	// CDO or VDO
	pDevice = pDriverObject->DeviceObject;
	if(!pDevice) return FALSE;

	while(pDevice)
	{
		pVpb = pDevice->Vpb;		
		if(pVpb && pVpb->Flags == VPB_MOUNTED)  // 마운트 플래그가 설정되어 있는지 여부를 판단한다.
		{
			LetterName.Buffer = NULL;
			if(ExGetPreviousMode() != KernelMode) 
			{

#if (NTDDI_VERSION >= NTDDI_WINXP)
				Status = IoVolumeDeviceToDosName( pDevice, &LetterName);
#else
				Status = RtlVolumeDeviceToDosName( pDevice, &LetterName);
#endif
				// VDO의 볼륨명을 비교한다. 마운트 되어 있을때만 유효하다. 
				if(RtlCompareUnicodeString( &DosDevicesName, &LetterName, TRUE ) == 0)
				{
					ExFreePool( LetterName.Buffer );
					LetterName.Buffer = NULL;
					return TRUE;
				}
				if(LetterName.Buffer)
				{
					ExFreePool( LetterName.Buffer );
					LetterName.Buffer = NULL;
				}
			}

		}
		pDevice = pDevice->NextDevice;  // 드랑이버오브젝트가 이전에 생성한 디바이스 오브젝트를 찾는다. // 물리디바이스나 가상디바이스를 나타낸다.
	}

	return FALSE;
} 
*/


BOOLEAN  
IS_DeviceStack_Mounted( IN PDEVICE_OBJECT pDevObj, IN OUT PDEVICE_OBJECT* ppDeviceObject )
{
	PVPB           pVpb             = NULL;
	PDEVICE_OBJECT pDeviceObject    = NULL;
	PDEVOBJ_EXT    pDeviceExtension = NULL;
		
	ASSERT( pDevObj && ppDeviceObject );
	if(!pDevObj || !ppDeviceObject ) return FALSE;	

	if(KeGetCurrentIrql() > APC_LEVEL) return FALSE;

	pDeviceObject = pDevObj;

	__try
	{

		while(pDeviceObject)
		{
			pVpb = pDeviceObject->Vpb;
			if(pVpb && (pVpb->Flags & VPB_MOUNTED))
			{
				*ppDeviceObject = pVpb->DeviceObject;
				return TRUE;
			}
			pDeviceExtension = (PDEVOBJ_EXT)pDeviceObject->DeviceObjectExtension;
			ASSERT( pDeviceExtension );
			if(!pDeviceExtension) return FALSE;
		
			// Lower DeviceObject
			pDeviceObject = pDeviceExtension->AttachedTo;
		}

	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		KdPrint(("IS_DeviceStack_Mounted Exception Occured. \n\n"));	
	}

	return FALSE;
}






// IRP_MJ_FILE_SYSTEM_CONTROL : IRP_MN_MOUNT_VOLUME 
// 으로 마운트된 Volume 의 드라이브 명을 구한다.

NTSTATUS 
GetVolumeDosName( IN PFLT_EXTENSION pDevExt )
{
	UNICODE_STRING  LetterName;
	NTSTATUS        Status = STATUS_SUCCESS;

	if(!pDevExt || pDevExt->wcVol != L'') return STATUS_INVALID_PARAMETER;
	if(!pDevExt->pRealDeviceObject || !pDevExt->pVolumeDeviceObject) return STATUS_INVALID_PARAMETER;
	if(KeGetCurrentIrql() >= DISPATCH_LEVEL) 
	{
		return STATUS_INVALID_LEVEL;
	}
	
	RtlInitUnicodeString( &LetterName, L"" );
	LetterName.Buffer = NULL;
	if(ExGetPreviousMode() != KernelMode) 
	{		
		if(g_MalwFind.ulOSVer >= OS_VER_WXP) 
		{
			Status = IoVolumeDeviceToDosName(  pDevExt->pRealDeviceObject, &LetterName );
			if(!NT_SUCCESS( Status )) 
			{
				KdPrint(("GetVolumeDosName >> IoVolumeDeviceToDosName > RealDeiveObject > Failed. Status=%08x \n", Status ));
				Status = IoVolumeDeviceToDosName( pDevExt->pVolumeDeviceObject, &LetterName );
			}
		}
		else
		{
			Status = RtlVolumeDeviceToDosName( pDevExt->pRealDeviceObject, &LetterName );
			if(!NT_SUCCESS( Status ))
			{
				KdPrint(("GetVolumeDosName >> RtlVolumeDeviceToDosName > RealDeiveObject > Failed. Status=%08x \n", Status ));
				Status = RtlVolumeDeviceToDosName( pDevExt->pVolumeDeviceObject, &LetterName );
			}
		}

		if(!NT_SUCCESS( Status ))
		{			
			if(LetterName.Buffer)
			{
				ExFreePool( LetterName.Buffer );		
				LetterName.Buffer = NULL;
			}
			KdPrint(("GetVolumeDosName >> IoVolumeDeviceToDosName Failed. Status=%08x \n", Status ));
			return Status;
		}

		// ToUpper
		pDevExt->wcVol = (WCHAR)toupper( LetterName.Buffer[0] );
		KdPrint(("GetVolumeDosName---pDevExt->wcVol=[%c:\\] \n", pDevExt->wcVol ));

		if(LetterName.Buffer)  ExFreePool( LetterName.Buffer );
		LetterName.Buffer = NULL;
	}

	return STATUS_SUCCESS;

}




ULONG  
GetFileRenameDestPath( IN PIO_STACK_LOCATION pIrpStack, 
					   IN PFILE_RENAME_INFORMATION  pRenameInfo, 
					   IN OUT PNAME_BUFFER pFileName  )
{
	ULONG         ulLength    = 0, ulOffset=0;
	NTSTATUS      Status      = STATUS_UNSUCCESSFUL;
	PFILE_OBJECT  pFileObject = NULL;
	PWCHAR        pPos        = NULL;

	if(!pIrpStack || !pRenameInfo || !pRenameInfo->FileName) return 0;

	pFileObject = pIrpStack->Parameters.SetFile.FileObject;

	FsRtlEnterFileSystem();
	ExAcquireResourceExclusiveLite( &g_MalwFind.LockResource, TRUE );
	
	// Simple Rename: SetFile.FileObject is NULL. 
    // Fully Qualified Rename: SetFile.FileObject is non-NULL and FILE_RENAME_INFORMATION.RootDir is NULL. 
    // Relative Rename: SetFile.FileObject and FILE_RENAME_INFORMATION.RootDir are both non-NULL. 
	__try
	{
		// Simple Rename: SetFile.FileObject is NULL. 
		if(!pFileObject)
		{			
			pPos = wcsrchr( pFileName->pBuffer, (int)L'\\');
			if(!pPos)
			{
				ExReleaseResourceLite( &g_MalwFind.LockResource );
				FsRtlExitFileSystem();
				return 0;
			}

			*(pPos++) = '\0';

			ulLength = pRenameInfo->FileNameLength >> 1;
			Status = RtlStringCchCatNW( pFileName->pBuffer, 
									    pFileName->ulMaxLength, 
										pRenameInfo->FileName, 
										ulLength );
			if(!NT_SUCCESS( Status ))
			{
				ExReleaseResourceLite( &g_MalwFind.LockResource );
				FsRtlExitFileSystem();
				return 0;
			}
			pFileName->ulLength = ulLength;

		}
		else
		{
			if(pRenameInfo->RootDirectory == NULL)
			{	// Fully Qualified Rename: SetFile.FileObject is non-NULL and FILE_RENAME_INFORMATION.RootDir is NULL. 

				if(pRenameInfo->FileName[0] != L'\\')
				{
					ExReleaseResourceLite( &g_MalwFind.LockResource );
					FsRtlExitFileSystem();
					return 0;
				}

				if(!_wcsnicmp(L"\\DosDevices\\", pRenameInfo->FileName, 12)) 
				{
					RtlZeroMemory( pFileName->pBuffer, (pFileName->ulMaxLength << 1) );
					ulOffset = 12;
				} 
				else if(!_wcsnicmp(L"\\??\\", pRenameInfo->FileName, 4))
				{
					RtlZeroMemory( pFileName->pBuffer, (pFileName->ulMaxLength << 1) );
					ulOffset = 4;
				}
				else
				{
					pFileName->pBuffer[2] = L'\0';
					RtlZeroMemory( &pFileName->pBuffer[2], ((pFileName->ulMaxLength-2) << 1) );
					ulOffset = 2;
				}
				
				ulLength = (pRenameInfo->FileNameLength >> 1) - ulOffset ;
				Status = RtlStringCchCopyNW( pFileName->pBuffer, 
											 pFileName->ulMaxLength, 
											 &pRenameInfo->FileName[ ulOffset ], 
											 ulLength );

				if(!NT_SUCCESS( Status ))
				{
					ExReleaseResourceLite( &g_MalwFind.LockResource );
					FsRtlExitFileSystem();
					return 0;
				}
				pFileName->ulLength = ulLength;			
			}
			else
			{	// Relative Rename: SetFile.FileObject and FILE_RENAME_INFORMATION.RootDir are both non-NULL. 
				WCHAR  DirBuffer[MAX_PATH] = L"";
				UNICODE_STRING  DirName;

				PFILE_OBJECT  pFileObj  = NULL;
				Status = ObReferenceObjectByHandle( pRenameInfo->RootDirectory,
													STANDARD_RIGHTS_REQUIRED, 
													NULL, 
													KernelMode, 
													(PVOID *)&pFileObj, 
													NULL );

				if(!NT_SUCCESS( Status )) 
				{
					ExReleaseResourceLite( &g_MalwFind.LockResource );
					FsRtlExitFileSystem();
					return 0;
				}

				RtlZeroMemory( DirBuffer, sizeof(DirBuffer) );
				RtlInitEmptyUnicodeString( &DirName, DirBuffer, sizeof(DirBuffer) );	
				
				FsdGetObjectName( pFileObj, &DirName );

				ObDereferenceObject( pFileObj );

				KdPrint(("%ws \n", DirName.Buffer ));

				ulLength = DirName.Length/sizeof(WCHAR);
				if(pFileName->pBuffer[ulLength] != L'\\')
				{			
					pFileName->pBuffer[ ulLength ] = L'\\';
					Status = RtlStringCchCatNW(  pFileName->pBuffer, 
												 pFileName->ulMaxLength,
												 DirName.Buffer,
												 ulLength  );
				}
				else
				{
					Status = RtlStringCchCatNW(  pFileName->pBuffer, 
												 pFileName->ulMaxLength,
												 DirName.Buffer,
												 ulLength  );
				}

				if(!NT_SUCCESS( Status ))
				{
					ExReleaseResourceLite( &g_MalwFind.LockResource );
					FsRtlExitFileSystem();
					return 0;
				}

			}
		}
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{

	}

	ExReleaseResourceLite( &g_MalwFind.LockResource );
	FsRtlExitFileSystem();

	return pFileName->ulLength;
}



// FileName FullPath  From FileObject
int 
GetFileFullPath( IN PDEVICE_OBJECT  pDeviceObject, 
				 IN PFILE_OBJECT    pFileObject, 
				 IN PFLT_EXTENSION  pDevExt, 
				 OUT PNAME_BUFFER   pFileName  )
{
	ULONG           nLength       = 0;
	NTSTATUS        Status        = STATUS_UNSUCCESSFUL;
	PFILE_OBJECT    pReFileObject = NULL;

	if(!pDeviceObject || !pDevExt || !pFileName || !pFileName->pBuffer ) 
	{
		return 0;
	}

	if(!MmIsAddressValid( pDeviceObject ) || !MmIsAddressValid( pDevExt ))
	{
		return 0;
	}

	if(!IS_FLT_DEVICE_TYPE( pDeviceObject->DeviceType ))  
	{   // CDROM DISK NETWORK  FILE_DEVICE_DISK FILE_DEVICE_CD_ROM
		return 0;
	}

	// Volume DeviceType			
	if(IS_VOLUME_DEVICE_TYPE( pDeviceObject->DeviceType ))
	{   
		if(pDevExt->wcVol == L'')
		{ // Volume Name

			KeEnterCriticalRegion();
			ExAcquireResourceExclusiveLite( &g_MalwFind.LockResource, TRUE );

			Status = GetVolumeDosName( pDevExt );
			if(!NT_SUCCESS( Status ))
			{
				KdPrint(("GetFileFullPath >> GetVolumeDosName Failed. \n"));
				ExReleaseResourceLite( &g_MalwFind.LockResource );
				KeLeaveCriticalRegion();
				return 0;
			}
			ExReleaseResourceLite( &g_MalwFind.LockResource );
			KeLeaveCriticalRegion();
		}

		pFileName->pBuffer[0] = pDevExt->wcVol;
		pFileName->pBuffer[1] = L':';
		pFileName->pBuffer[2] = L'\\';		
		pFileName->ulLength += 3;
	}

	// FileName Too Short
	if(!pFileObject || !MmIsAddressValid( pFileObject ) || !pFileObject->FileName.Buffer || !pFileObject->FileName.Length)  
	{
		return pFileName->ulLength;
	}

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &g_MalwFind.LockResource, TRUE );

	__try
	{				
		if(pFileObject->FileName.Buffer[0] == L'\\')
		{
			if(IS_VOLUME_DEVICE_TYPE( pDeviceObject->DeviceType ))
			{
				pFileName->pBuffer[2] = L'\0';
				pFileName->ulLength--;
			}
			else if(IS_NETWORK_DEVICE_TYPE( pDeviceObject->DeviceType ))
			{
				pFileName->pBuffer[0] = L'\\';
				pFileName->pBuffer[1] = L'\0';
				pFileName->ulLength += 2;
			}

			nLength = pFileObject->FileName.Length >> 1;		
			if(nLength < pFileName->ulMaxLength)
			{
				RtlStringCchCatNW( pFileName->pBuffer, pFileName->ulMaxLength, pFileObject->FileName.Buffer, nLength );					
				pFileName->ulLength += nLength;
			}
		}
		else
		{
			//
			// 2013.10.23 Written by tdev
			// MmIsAddressValid( &pReFileObject->FileName ) 추가함
			//
			pReFileObject = pFileObject->RelatedFileObject;	
			if(pReFileObject && MmIsAddressValid( pReFileObject ) && MmIsAddressValid( &pReFileObject->FileName ) )
			{
				//
				// 2013.10.23 Written by tdev
				// Tsis 에서 아래의 포인터는 있는데 페이징이 발생한 경우가 있어서 추가합니다.
				// MmIsAddressValid( pReFileObject->FileName.Buffer ) 추가함
				//
				nLength = pReFileObject->FileName.Length;
				if(pReFileObject->FileName.Buffer && MmIsAddressValid( pReFileObject->FileName.Buffer ) && nLength > 0)
				{		
					nLength = pReFileObject->FileName.Length >> 1;	
					if(nLength < pFileName->ulMaxLength)
					{
						if(pReFileObject->FileName.Buffer[0] == L'\\')
						{
							RtlStringCchCatNW( pFileName->pBuffer, pFileName->ulMaxLength, &pReFileObject->FileName.Buffer[1], nLength-1 );		
							pFileName->ulLength += (nLength -1);
						}
						else
						{
							RtlStringCchCatNW( pFileName->pBuffer, pFileName->ulMaxLength, pReFileObject->FileName.Buffer, nLength );		
							pFileName->ulLength += nLength;
						}
					}

					if(MmIsAddressValid(pReFileObject) && MmIsAddressValid(pReFileObject->FileName.Buffer))
					{
						nLength = (pReFileObject->FileName.Length/sizeof(WCHAR))-1;			
						if(nLength < pFileName->ulMaxLength && pReFileObject->FileName.Buffer[ nLength ] != L'\\')
						{
							RtlStringCchCatNW( pFileName->pBuffer, pFileName->ulMaxLength, L"\\", (sizeof(WCHAR) >> 1) );								
							pFileName->ulLength++;
						}
					}
				}
				else 
				{
					if(IS_NETWORK_DEVICE_TYPE( pDeviceObject->DeviceType ))
					{
						RtlStringCchCatNW( pFileName->pBuffer, pFileName->ulMaxLength, L"\\", (sizeof(WCHAR) >> 1) );										
						pFileName->ulLength++;
					}
				}				
			}

			nLength = pFileObject->FileName.Length >> 1;		
			if(nLength < pFileName->ulMaxLength)
			{
				RtlStringCchCatNW( pFileName->pBuffer, pFileName->ulMaxLength, pFileObject->FileName.Buffer, nLength  );		
				pFileName->ulLength += nLength;
			}
		}

		if(pFileName->ulLength < 3) 
		{
			ExReleaseResourceLite( &g_MalwFind.LockResource );
			KeLeaveCriticalRegion();
			return 0;   
		}

		if(IS_NETWORK_DEVICE_TYPE( pDeviceObject->DeviceType ))
		{   //   /;Z:\   //;Z:

			if(pFileName->pBuffer[1] == L';')
			{   // Win2K-style name. Grab the drive letter and skip over the share
				ULONG  nSlashes = 0;
				PWCHAR pPtr = NULL;

				if(!_wcsnicmp(&pFileName->pBuffer[2], L"Csc", wcslen(L"Csc")) || !_wcsnicmp(&pFileName->pBuffer[2], L"LanmanRedirector", wcslen(L"LanmanRedirector")) )
				{
				}
				else
				{
					pFileName->pBuffer[0] = pFileName->pBuffer[2];
					pFileName->pBuffer[1] = L':';
					pFileName->pBuffer[2] = L'\\';
					pPtr = &pFileName->pBuffer[3];

					while( *pPtr && nSlashes != 3 )
					{
						if( *pPtr == L'\\' ) nSlashes++;
						pPtr++;
					}
					RtlStringCchCopyW( &pFileName->pBuffer[3], pFileName->ulMaxLength-5, pPtr );
				}
			} 	
			else if(pFileName->pBuffer[2] == L';')
			{   
				int    nPosIndex   = 0;
				ULONG  nSlashes = 0;
				PWCHAR pPtr = NULL;

				if(!_wcsnicmp( pFileName->pBuffer, L"\\\\;LanmanRedirector\\;", wcslen( L"\\\\;LanmanRedirector\\;" )))
				{   // Windows 7 공유폴더 패킷은 이런식으로 들어온다.
					nPosIndex = wcslen( L"\\\\;LanmanRedirector\\;");
					pFileName->pBuffer[0] = pFileName->pBuffer[ nPosIndex ];
					pFileName->pBuffer[1] = L':';
					pFileName->pBuffer[2] = L'\\';

					pPtr = &pFileName->pBuffer[ nPosIndex+1 ];
					while( *pPtr && nSlashes != 3 )
					{
						if( *pPtr == L'\\' ) nSlashes++;
						pPtr++;
					}
					RtlStringCchCopyW( &pFileName->pBuffer[3], pFileName->ulMaxLength-5, pPtr );
				}
			    else if( !_wcsnicmp(&pFileName->pBuffer[3], L"Csc", wcslen(L"Csc")) || !_wcsnicmp(&pFileName->pBuffer[3], L"LanmanRedirector", wcslen(L"LanmanRedirector")) )
				{   // //;Csc\.\  // //;LanmanRedirector\.\						
				}
				else
				{
					pFileName->pBuffer[0] = pFileName->pBuffer[3];
					pFileName->pBuffer[1] = L':';
					pFileName->pBuffer[2] = L'\\';
					pPtr = &pFileName->pBuffer[4];

					while( *pPtr && nSlashes != 3 )
					{
						if( *pPtr == L'\\' ) nSlashes++;
						pPtr++;
					}
					RtlStringCchCopyW( &pFileName->pBuffer[3], pFileName->ulMaxLength-5, pPtr  );
				}
			}
			else if( pFileName->pBuffer[2] == L':' ) 
			{   // NT 4-style name. Skip the share name 			
				ULONG  nSlashes = 0;
				PWCHAR pPtr = NULL;

				pFileName->pBuffer[0] = pFileName->pBuffer[1];
				pFileName->pBuffer[1] = L':';
				pFileName->pBuffer[2] = L'\\';

				// The second slash after the drive's slash (x:\)  is the start of the real path
				pPtr = &pFileName->pBuffer[3];
				while( *pPtr && nSlashes != 3 )
				{
					if( *pPtr == L'\\' ) nSlashes++;
					pPtr++;
				}
				RtlStringCchCopyW( &pFileName->pBuffer[3], pFileName->ulMaxLength-5, pPtr );
			} 			 
			else 
			{
				;
			}
		}

		if(wcsstr(pFileName->pBuffer, L"$DATA") || wcsstr(pFileName->pBuffer, L"IPC$"))  
		{
			ExReleaseResourceLite( &g_MalwFind.LockResource );
			KeLeaveCriticalRegion();
			return 0;
		}

		// 파일이라면 위치정보 찾기 :: 파일명 위치찾기
		pFileName->pFilePos = wcsrchr( pFileName->pBuffer, L'\\' );
		pFileName->pFilePos++;

	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		KdPrint(("GetFileFullPath: Exception Occured! \n "));
	}
	
	ExReleaseResourceLite( &g_MalwFind.LockResource );
	KeLeaveCriticalRegion();
	return (int)pFileName->ulLength;

}





// Same DeviceObject ?

BOOLEAN  
FsdEqualDeviceStack( IN PDEVICE_OBJECT pBaseDevObj, IN PDEVICE_OBJECT pFindDevObj )
{
	PDEVICE_OBJECT pDevObj = NULL;

	ASSERT( pBaseDevObj && pFindDevObj );
	if(!pBaseDevObj || !pFindDevObj) return FALSE;

	pDevObj = pBaseDevObj;
	while(pDevObj)
	{
		if(pDevObj == pFindDevObj)
		{
			//KdPrint(("FsdEqualDeviceStack: TRUE \n"));
			return TRUE;
		}
		pDevObj = pDevObj->AttachedDevice;
	}
	return FALSE;
}



// NHCAFLT DeviceObject 가 어태치 되어 있는지 검사하는 루틴
BOOLEAN  
Attached_MalwFind_DeviceObject( IN PDEVICE_OBJECT pVolDeviceObject )
{
	PDEVICE_OBJECT  pUpperDevObj = NULL;

	pUpperDevObj = pVolDeviceObject;	
	if(!pUpperDevObj) 
	{
		ASSERT( pUpperDevObj );
		return FALSE;
	}

	while(pUpperDevObj)
	{
		if(IS_MALFIND_DEVICE_OBJECT( pUpperDevObj ))
		{
			return TRUE;
		}
		pUpperDevObj = pUpperDevObj->AttachedDevice;
	}
			
	return FALSE;

}




#if (NTDDI_VERSION < NTDDI_WINXP)

NTSTATUS  IoGetDiskDeviceObject_2K( IN PDEVICE_OBJECT pFsDeviceObject, OUT PDEVICE_OBJECT* ppDiskDeviceObject)
{
    PVPB      pVpb    = NULL;
    NTSTATUS  Status  = STATUS_SUCCESS;

	ASSERT( pFsDeviceObject && ppDiskDeviceObject );
	if(!pFsDeviceObject || !ppDiskDeviceObject ) 
	{
		KdPrint(("STATUS_INVALID_PARAMETER >> IoGetDiskDeviceObject_2K 1. \n"));
		return STATUS_INVALID_PARAMETER;
	}
   
	pVpb = pFsDeviceObject->Vpb;
    if(pVpb)
    {
        if(pVpb->Flags & VPB_MOUNTED)
        {        
            *ppDiskDeviceObject = pVpb->RealDevice;
            ObReferenceObject( pVpb->RealDevice );
            Status = STATUS_SUCCESS;
        }
        else
        {
            Status = STATUS_VOLUME_DISMOUNTED;
        }
    }
    else
    {       
		KdPrint(("STATUS_INVALID_PARAMETER >> IoGetDiskDeviceObject_2K 2. \n"));
        Status = STATUS_INVALID_PARAMETER;
    }

	return Status;

}

#endif



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////




NTSTATUS
GetSystemRootDosName( IN OUT PWCHAR pzOutTarget, ULONG ulMaxTarget )
{
    static WCHAR        TagetNameBuffer[50];
    UNICODE_STRING      TargetName;
	UNICODE_STRING      SrcName;

    HANDLE              hLink =  NULL;
    HANDLE              hDir  =  NULL;
	OBJECT_ATTRIBUTES   ObjectAttr;
	NTSTATUS            Status = STATUS_UNSUCCESSFUL;

	if(!pzOutTarget || !ulMaxTarget) 
	{
		KdPrint(("STATUS_INVALID_PARAMETER >> GetSystemRootDosName 1. \n"));
		return STATUS_INVALID_PARAMETER;
	}

    RtlInitUnicodeString( &SrcName, (PWCHAR)L"\\" );    
	InitializeObjectAttributes( &ObjectAttr, &SrcName, OBJ_KERNEL_HANDLE, (HANDLE)NULL, (PSECURITY_DESCRIPTOR)NULL );
	
	Status = ZwOpenDirectoryObject( &hDir, DIRECTORY_QUERY, &ObjectAttr );
    if(!NT_SUCCESS( Status ))  return Status;

    // Open symbolic link object(s)    
	RtlInitUnicodeString( &SrcName, L"SystemRoot" );
    InitializeObjectAttributes( &ObjectAttr, &SrcName, OBJ_KERNEL_HANDLE, (HANDLE)hDir, (PSECURITY_DESCRIPTOR)NULL );

    Status = ZwOpenSymbolicLinkObject( &hLink, SYMBOLIC_LINK_QUERY, &ObjectAttr );
	if(!NT_SUCCESS( Status ))
	{
		ZwClose( hDir );  
		return Status;
	}

	RtlZeroMemory( TagetNameBuffer, sizeof(TagetNameBuffer) );
	RtlInitUnicodeString( &TargetName, TagetNameBuffer );
	Status = ZwQuerySymbolicLinkObject( hLink, &TargetName, NULL );
	if(NT_SUCCESS( Status ))
	{
		RtlStringCchCopyW( pzOutTarget, ulMaxTarget, TargetName.Buffer );
	}

	ZwClose(hLink);   
    ZwClose( hDir );  
    return Status;

}   



//***************************************************************************************
//* NAME:			GetSymbolicLink
//*
//* DESCRIPTION:	Given a symbolic link name this routine returns a string with the 
//*					links destination and a handle to the open link name.
//*					
//*	PARAMETERS:		SymbolicLinkName		IN
//*					SymbolicLink			OUT
//*					LinkHandle				OUT
//*
//*	IRQL:			IRQL_PASSIVE_LEVEL.
//*
//*	RETURNS:		STATUS_SUCCESS
//*					STATUS_UNSUCCESSFUL
//*
//* NOTE:			Caller must free SymbolicLink->Buffer AND close the LinkHandle 
//*					after a successful call to this routine.
//***************************************************************************************
NTSTATUS 
GetSymbolicLink( IN  PUNICODE_STRING SymbolicLinkName,
			     OUT PUNICODE_STRING SymbolicLink,
			     OUT PHANDLE  pOutLinkHandle  )
{
	
	NTSTATUS          Status    = STATUS_UNSUCCESSFUL;
	NTSTATUS          RetStatus = STATUS_UNSUCCESSFUL;
	ULONG	          ulSymbolicLinkLength = 0;
	HANDLE	          hTmpLinkHandle       = NULL;
	UNICODE_STRING	  uniTempSymbolicLink;
	OBJECT_ATTRIBUTES ObjAttr;

	//
	// Open and query the symbolic link
	//
	InitializeObjectAttributes( &ObjAttr, SymbolicLinkName, OBJ_KERNEL_HANDLE, NULL, NULL );
	Status = ZwOpenSymbolicLinkObject( &hTmpLinkHandle, GENERIC_READ, &ObjAttr );
	if(STATUS_SUCCESS == Status)
	{
		//
		// Get the size of the symbolic link string
		//
		ulSymbolicLinkLength			  = 0;
		uniTempSymbolicLink.Length		  = 0;
		uniTempSymbolicLink.MaximumLength = 0;

		Status = ZwQuerySymbolicLinkObject( hTmpLinkHandle, &uniTempSymbolicLink, &ulSymbolicLinkLength );
		if(STATUS_BUFFER_TOO_SMALL == Status && ulSymbolicLinkLength > 0)
		{
			// Allocate the memory and get the ZwQuerySymbolicLinkObject		
			uniTempSymbolicLink.Buffer = ExAllocatePoolWithTag( NonPagedPool, ulSymbolicLinkLength, MALWFIND_NAME_TAG );
			uniTempSymbolicLink.Length = 0;
			uniTempSymbolicLink.MaximumLength = (USHORT)ulSymbolicLinkLength;

			Status = ZwQuerySymbolicLinkObject( hTmpLinkHandle, &uniTempSymbolicLink, &ulSymbolicLinkLength );
			if(STATUS_SUCCESS == Status)
			{
				SymbolicLink->Buffer  = uniTempSymbolicLink.Buffer;
				SymbolicLink->Length  = uniTempSymbolicLink.Length;
				if(pOutLinkHandle)
				{
					(*pOutLinkHandle) = hTmpLinkHandle;
				}
				SymbolicLink->MaximumLength = uniTempSymbolicLink.MaximumLength;
				RetStatus  = STATUS_SUCCESS;
			}
			else
			{
				ExFreePool( uniTempSymbolicLink.Buffer );
				uniTempSymbolicLink.Buffer = NULL;
			}
		}
	}

	return RetStatus;

}



//***************************************************************************************
//* NAME:			ExtractDriveString
//*
//* DESCRIPTION:	Extracts the drive from a string.  Adds a NULL termination and 
//*					adjusts the length of the source string.
//*					
//*	PARAMETERS:		Source				IN OUT
//*
//*	IRQL:			IRQL_PASSIVE_LEVEL.
//*
//*	RETURNS:		STATUS_SUCCESS
//*					STATUS_UNSUCCESSFUL
//***************************************************************************************

NTSTATUS 
ExtractDriveString_Dir4( IN OUT PUNICODE_STRING pSource )
{
	NTSTATUS  Status = STATUS_UNSUCCESSFUL;
	ULONG	  i = 0;
	ULONG	  NumSlashes = 0;

	if(!pSource) 
	{
		KdPrint(("STATUS_INVALID_PARAMETER >> ExtractDriveString_Dir4 1. \n"));
		return STATUS_INVALID_PARAMETER;
	}

	while( ((i*2) < pSource->Length) && (4 != NumSlashes) )
	{
		if( L'\\' == pSource->Buffer[i] )
		{
			NumSlashes++;
		}
		i++;
	}

	if( (4 == NumSlashes ) && (i>1) )
	{
		i--;
		pSource->Buffer[i]	= L'\0';
		pSource->Length		= (USHORT) i * 2;
		Status				= STATUS_SUCCESS;
	}

	return Status;

}


NTSTATUS 
ExtractDriveString_Dir3( IN OUT PUNICODE_STRING pSource )
{
	NTSTATUS  Status = STATUS_UNSUCCESSFUL;
	ULONG	  i = 0;
	ULONG	  NumSlashes = 0;

	if(!pSource)
	{
		KdPrint(("STATUS_INVALID_PARAMETER >> ExtractDriveString_Dir3 1. \n"));
		return STATUS_INVALID_PARAMETER;
	}

	while( ((i*2) < pSource->Length) && (3 != NumSlashes) )
	{
		if( L'\\' == pSource->Buffer[i] )
		{
			NumSlashes++;
		}
		i++;
	}

	if( (3 == NumSlashes ) && (i>1) )
	{
		i--;
		pSource->Buffer[i]	= L'\0';
		pSource->Length		= (USHORT) i * 2;
		Status				= STATUS_SUCCESS;
	}

	return Status;

}

//***************************************************************************************
//* NAME:			GetSystemRootPath
//*
//* DESCRIPTION:	On success this routine allocates and copies the system root path 
//*					(ie C:\Windows) into the SystemRootPath parameter
//*					
//*	PARAMETERS:		SystemRootPath				out
//*
//*	IRQL:			IRQL_PASSIVE_LEVEL.
//*
//*	RETURNS:		STATUS_SUCCESS
//*					STATUS_UNSUCCESSFUL
//*
//* NOTE:			Caller must free SystemRootPath->Buffer after a successful call to 
//*					this routine.
//***************************************************************************************


#define SYSTEM_ROOT	 L"\\SystemRoot"


NTSTATUS
GetSystemRootPath( IN OUT PWCHAR pwzSystemRootPath, IN ULONG ulMaxSystemRootPath )
{
	NTSTATUS		Status    = STATUS_UNSUCCESSFUL;
	NTSTATUS		RetStatus = STATUS_UNSUCCESSFUL;

	UNICODE_STRING	SysRootName;
	UNICODE_STRING	SysRootSymLink1;
	UNICODE_STRING	SysRootSymLink2;
	UNICODE_STRING	SysDosRootPath;

	PDEVICE_OBJECT	pDeviceObject    = NULL;
	PFILE_OBJECT	pFileObject      = NULL;
	HANDLE			hLinkHandle      = NULL;
	ULONG			ulFullPathLength = 0;

	ASSERT( pwzSystemRootPath && ulMaxSystemRootPath );
	if(!pwzSystemRootPath | !ulMaxSystemRootPath) 
	{
		KdPrint(("STATUS_INVALID_PARAMETER >> ExtractDriveString_Dir3 1. \n"));
		return STATUS_INVALID_PARAMETER;
	}

	__try
	{
		RtlInitUnicodeString( &SysRootName, SYSTEM_ROOT );
		// Get the full path for the system root directory
		Status = GetSymbolicLink( &SysRootName, &SysRootSymLink1, &hLinkHandle );
		if(STATUS_SUCCESS == Status)
		{
			// At this stage we have the full path but its in the form:
			// \Device\Harddisk0\Partition1\WINDOWS lets try to get the symoblic name for this drive so it looks more like c:\WINDOWS.

			KdPrint(("Full System Root Path: %ws\n", SysRootSymLink1.Buffer ));
			ulFullPathLength = SysRootSymLink1.Length;
			ZwClose( hLinkHandle );
			hLinkHandle = NULL;

			// Remove the path so we can query the drive letter
			Status = ExtractDriveString_Dir4( &SysRootSymLink1 );
			if( STATUS_SUCCESS != Status ) 
			{
				Status = ExtractDriveString_Dir3( &SysRootSymLink1 );
			}

			if( STATUS_SUCCESS == Status )
			{
				// We've added a NULL termination character so we must reflect that in the total length.
				ulFullPathLength = ulFullPathLength - 2;
				// Query the drive letter		
				Status = GetSymbolicLink( &SysRootSymLink1, &SysRootSymLink2, &hLinkHandle );
				if(STATUS_SUCCESS == Status)
				{
					Status = IoGetDeviceObjectPointer( &SysRootSymLink2, SYNCHRONIZE | FILE_ANY_ACCESS, &pFileObject, &pDeviceObject );
					if(STATUS_SUCCESS == Status)
					{
						ObReferenceObject( pDeviceObject );
						// Get the dos name for the drive
						Status = RtlVolumeDeviceToDosName( pDeviceObject, &SysDosRootPath );
						if(STATUS_SUCCESS == Status && NULL != SysDosRootPath.Buffer )
						{										
							RtlStringCchCopyNW( pwzSystemRootPath, ulMaxSystemRootPath, SysDosRootPath.Buffer, SysDosRootPath.Length );					
							RtlStringCchCatNW(  pwzSystemRootPath, ulMaxSystemRootPath, L"\\", wcslen(L"\\") );
							RtlStringCchCatNW( pwzSystemRootPath, ulMaxSystemRootPath, SysRootSymLink1.Buffer+(SysRootSymLink1.Length/2)+1, ulFullPathLength - SysRootSymLink1.Length  );

							ExFreePoolWithTag( SysDosRootPath.Buffer, MALWFIND_NAME_TAG );
							SysDosRootPath.Buffer = NULL;
							RetStatus = STATUS_SUCCESS;
						}
						ObDereferenceObject( pDeviceObject );
					}

					ZwClose( hLinkHandle );
					if(SysRootSymLink2.Buffer)
					{
						ExFreePoolWithTag( SysRootSymLink2.Buffer, MALWFIND_NAME_TAG );
					}

				}
			}

			if(SysRootSymLink1.Buffer)
			{
				ExFreePoolWithTag( SysRootSymLink1.Buffer, MALWFIND_NAME_TAG );
			}

		}

	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{

	}

	return RetStatus;

}



