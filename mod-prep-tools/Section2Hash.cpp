// Section2Hash.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <iostream>
#include <string>
#include <string.h>
#include <windows.h>
using namespace std;
//DRM struct definitions from alphaZomega's ROTTR_drm.bt
struct {
    uint32_t m_version;
    uint32_t m_nameSize;    //After drmEntry1
    uint32_t m_nameSize2;   //After m_nameSize
    uint32_t m_unk00;       //Reserved?
    uint32_t m_unk01;       //Reserved?
    uint32_t m_unk02;       //Reserved?
    uint32_t m_numSections;
    uint32_t m_unk03;       //Read request section
} drm_header;
struct {
    uint32_t m_fileSize;		    //Section size - relocationDataSize
    uint32_t m_type;
    uint32_t m_relocationSize : 24;	//&0xFF000000 = numRelocations. relocationDataSize = & 0xFFFFFF00
    uint32_t m_numRelocations : 8;
    uint32_t m_hash;
    uint32_t m_unk;
} drmEntry;



#define VERSION "1.0"
int main(int argc, char* argv[])
{
    printf("Section2Hash Version %s: Change section file name to contain hash and uncompressed file size.\n"
        "Input section file must have section number in their name. "
        "i.e. : 'Section 123.tr2mesh' ,'Replace_123.tr2pcd'\n", VERSION);
    if (argc < 3)
    {
        printf("Usage: Section2Hash  <drm file>  <section file>\n");
        return 0;
    }

    struct stat stat_buf;
    int rc = stat(argv[2], &stat_buf);
    if (rc != 0)
    {
        printf("Error: Cannot get section file size\n");
        return 0;
    } 
    unsigned long file_size = stat_buf.st_size;

    char sec_file[256];
    char sec_path[256];
    char sec_ext[256];
    // find start of file name
    string filepath = argv[2];
    char* path_end = strrchr(argv[2], '\\');
    if (!path_end)
    {
        path_end = strrchr(argv[2], '/');
    }
    if (path_end)
    {
        strcpy_s(sec_file, 256, path_end + 1);
        *(path_end + 1) = 0;
        strcpy_s(sec_path, 256, argv[2]);
    }
    else
    {
        strcpy_s(sec_file,256,argv[2]);
        strcpy_s(sec_path, 256, "");
    }
    // seperate filename and extension
    char *ext = strrchr(sec_file, '.');
    if (ext)
    {
        strcpy_s(sec_ext, 256, ext);
        *ext = '\0';
    }
    else
    {
        strcpy_s(sec_ext, 256, "");
    }
        
    // get number from section file name
    char* num_start_pos,*num_end_pos;
    num_start_pos = sec_file;            // section file
    while (*num_start_pos && !isdigit(*num_start_pos))
        num_start_pos++;
    num_end_pos = num_start_pos;
    while (*num_end_pos !=' .' && isdigit(*num_end_pos))
        num_end_pos++;

    if (num_end_pos == num_start_pos)
    {
        printf("Error: Cannot find number in section file name\n");
        return 0;
    }

    *num_end_pos = '\0';
    unsigned int section_no = atoi(num_start_pos);
    printf("section no: %d, ext: %s\n", section_no, sec_ext);

    FILE* fp;
    fopen_s(&fp, argv[1], "rb");
    if (!fp)
    {
        printf("Error: Cannot open DRM file\n");
        return 0;
    }
    while (true)
    {

        fread(&drm_header, sizeof(drm_header), 1, fp);
        if (drm_header.m_version != 0x16)
        {
            printf("Error: Unsupported DRM version\n");
            break;
        }
        if (section_no > drm_header.m_numSections)
        {
            printf("Error: Section number greater than total DRM sections\n");
            break;
        }
        fseek(fp, (section_no - 1) * sizeof(drmEntry), SEEK_CUR);
        fread(&drmEntry, sizeof(drmEntry), 1, fp);
        unsigned long unique_id = drmEntry.m_hash | ((unsigned long)drmEntry.m_type<<24);
        printf("hash id: 0x%08x\n", unique_id);

        // change section file name, keep original file extension string
        char new_fname[256];
        string OutputFolder = ".\\mod_out\\";
        if (CreateDirectoryA(OutputFolder.c_str(), NULL) ||
            ERROR_ALREADY_EXISTS == GetLastError())
        {
            //sprintf_s(new_fname, 256, "%s%08x_%08x%s", sec_path, unique_id, file_size, sec_ext);
            sprintf_s(new_fname, 256, "%s%08x_%08x%s", OutputFolder.c_str(), unique_id, file_size, sec_ext);

            printf("new filename %s\n",new_fname);
            rename(argv[2], new_fname);
            // CopyFile(...)
        }
        else
        {
            // Failed to create directory.
            printf("Error: Cannot create directory %s\n", OutputFolder.c_str());
        }
        break;
    }
    fclose(fp);
    return 0;
}
