#pragma once

namespace cdc
{
#if ROTTR
	struct SectionInfo
	{
		int size;
		unsigned char type;
		struct /* bitfield */
		{
			char skip : 1; /* bit position: 0 */
			char tigerPatch : 1; /* bit position: 1 */
			char mergedSubSections : 1; /* bit position: 2 */
			char __free : 5; /* bit position: 3 */
		}; /* bitfield */
		unsigned short versionID;
		union
		{
			struct /* bitfield */
			{
				unsigned int hasDebugInfo : 1; /* bit position: 0 */
				unsigned int resourceType : 7; /* bit position: 1 */
				unsigned int relocationTableSize : 24; /* bit position: 8 */
			}; /* bitfield */
			unsigned int packedAsInt;
		}; 
		int id;
		unsigned int specMask;
	}; 

	struct SectionExtraInfo
	{
		union
		{
			const unsigned __int64 kArchiveIndexBits;
			const unsigned __int64 kArchiveIndexMask;
			const unsigned __int64 kDLCIndexBits;
			const unsigned __int64 kDLCIndexMask;
			const unsigned __int64 kOffsetMask;
			unsigned int uniqueId;
		}; 
		unsigned __int64 packedOffset;
		unsigned int compressedSize;
		unsigned int decompressedOffset;
	}; 
	class ResolveReceiver
	{
	public:
		long Padding_1937[6];
		long* padding1;// ResolveObject* m_pObject;
		long* padding2;// SectionRecord* m_pReleaseRecord;
		long* padding3[7];// class cdc::TigerSectionLoader m_tigerLoader;
		char m_specMaskReload;
		char m_update;
		struct 
		{
			char m_isHighPriority : 1; /* bit position: 0 */
			char m_loading : 1; /* bit position: 1 */
			char m_canceled : 1; /* bit position: 2 */
			char m_done : 1; /* bit position: 3 */
			char m_inReceiveData : 1; /* bit position: 4 */
		}; 
		char Padding_1938;
		int m_state;
		int m_numSections;
		int m_numReleaseRecords;
		int m_primSectionRtrId;
		long Padding_1939;
		SectionInfo* m_section;
		SectionExtraInfo* m_sectionEx;
		unsigned int* m_rtrID;
		int m_sectionIndex;
		int m_bodyLen;
		int m_resolveLen;
		long Padding_1940;
		unsigned char* m_resolvePtr;
		void* m_resolveTable;
		int m_dependentDrmLength;
		long Padding_1941;
		char* m_pDependentDrmList;
		int m_dependentDrmRemaining;
		int m_includedDrmLength;
		char* m_pIncludedDrmList;
		int m_includedDrmRemaining;
		int m_paddingRemaining;
		int m_postPaddingState;
		enum SectionType m_reloadSectionType;
		unsigned int m_flags;
		long Padding_1942;
		union
		{
			struct
			{
				void* m_pPreviousRecord;
				unsigned int m_previousSpecMask;
			}; 
			struct
			{
				void* m_retFunc;
				void* m_retData;
				void* m_retData2;
				void** m_retPointer;
				void* m_cancelCB;
				void* m_cancelCBParam;
				void* padding4;// class cdc::SList<cdc::ResolveReceiver::CallbackInfo> m_RetCB;
			};
		}; 
	}; 
#endif

	class FileSystem
	{
	public:
		virtual void* RequestRead(void* receiver, const char* name, unsigned int startOffset) = 0;
		virtual void* OpenFile(char const* fileName) = 0;
		virtual bool FileExists(char const* fileName) = 0;
		virtual unsigned int GetFileSize(char const* fileName) = 0;
		virtual void SetSpecialisationMask(unsigned int specMask) = 0;
		virtual unsigned int GetSpecialisationMask() = 0;
		virtual int GetStatus() = 0;
		virtual void Update() = 0;
		virtual void Synchronize() = 0;
#if TR8
		virtual void Suspend() = 0;
		virtual bool Resume() = 0;
		virtual bool IsSuspended() = 0;
#endif
	};

	struct MSFileSystem
	{
		struct Request
		{
			char pad1[20];
			char m_pFileName[128];
			char pad2[20];
			unsigned int m_bytesRead;
			unsigned int m_bytesProcessed;
			int m_readState;
			unsigned int m_offset;
			unsigned int m_size;
			Request* m_next;
		};

		char pad[1099812];
		Request* m_queue;
		Request* m_free;
		unsigned int m_numUsedRequests;
	};
}

cdc::FileSystem* CreateHookFileSystem(cdc::FileSystem* pDiskFS);
cdc::FileSystem* CreateMultiFileSystem(cdc::FileSystem* pFS, cdc::FileSystem* pDiskFS);

#define g_pDiskFS VAR_U_(DISKFS, cdc::MSFileSystem*)
#define g_pFS VAR_U_(ARCHIVEFS, cdc::FileSystem*)

cdc::FileSystem* GetFS();