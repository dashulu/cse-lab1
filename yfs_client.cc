// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

yfs_client::yfs_client()
{
    ec = new extent_client();

}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    printf("isfile: %lld is a dir\n", inum);
    return false;
}

bool
yfs_client::isdir(inum inum)
{
    return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */

    printf("yfs_client setattr ino:%d size:%d\n", ino, size);
    std::string file;
    if((r = ec->get(ino, file)) != yfs_client::OK) {
        return r;
    }

    printf("file.size in setattr:%d \n", file.size());
    if(file.size() > size) {
        file.resize(size);
        printf("file.size in setattr:%d \n", file.size());
    } else {
        for(int i = file.size();i < size;i++) {
            file.push_back('\0');
        }
    }

    r = ec->put(ino, file);
    struct stat st;
    extent_protocol::attr a;
    ec->getattr(ino, a);
    printf("after yfs_client setattr ino:%d size:%d\n", ino, a.size);
    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    bool found = false;
    std::list<dirent> list;
    std::list<dirent> list_readdir;
    yfs_client::inum ino;
    yfs_client::dirent dentry;
    if(( r = lookup(parent, name, found, ino_out)) != OK) {
        ino_out = 0;
        return r;
    } 
    if(found) {
        ino_out = 0;
        return r;
    }

    if(isdir(parent)) {
        readdir(parent, list);
        dentry.name.assign(name);
        ec->create(extent_protocol::T_FILE, dentry.inum);
        ino_out = dentry.inum;
        list.push_back(dentry);
        printf("dentry name:%s  inum:%d\n", dentry.name.c_str(), dentry.inum);
        std::string content;
        ec->get(parent, content);
        printf("get content of data:%s\n", content.c_str());
        content = dentry_list_to_string(list);
        printf("content of dentry:%s\n", content.c_str());
        ec->put(parent, dentry_list_to_string(list));
        ec->get(parent, content);
        printf("get content of data:%s\n", content.c_str());
        bool found = false;
        lookup(parent, dentry.name.c_str(), found, ino);
        if(found) {
            printf("found file %s in parent %d\n", dentry.name.c_str(), parent);
        }
        readdir(parent, list_readdir);
        if(list_readdir.size() > 0) {
            printf("list_readdir size:%d\n", list_readdir.size());
        } else {
            printf("readdir has some error.\n");
        }
    } else {
        return r;
    }
    return r;
}

std::string i_to_str(int value) {
    std::string str;
    char ch;
    do {
        str.push_back(value % 10 + 48);
        value /= 10;
    } while(value != 0);
    for(uint32_t i = 0;i < str.length()/2;i++) {
        ch = str[i];
        str[i] = str[str.length() - i - 1];
        str[str.length() - i - 1] = ch;
    }
    return str;
}


std::string 
yfs_client::dentry_list_to_string(std::list<dirent> list) {
    std::string content;
    for(std::list<dirent>::iterator iter = list.begin();iter != list.end();iter++) {
        printf("iter value:%s\n", iter->name.c_str());
        content.append((*iter).name + " ");
        content.append(i_to_str((*iter).inum) + " ");
    } 
    return content;
}

// dentry is organised as below:
// name + ' ' + inum + ' '
// find a dentry from the begin-th pos of dir_content
// return -1 if there is not a suitable dentry
// return the start pos of next dentry. 
int 
yfs_client::get_dentry(std::string dir_content, int begin, yfs_client::dirent &dentry) {
    dentry.inum = 0;
    dentry.name.clear();
    //get file name
    while(begin < dir_content.size() && dir_content[begin] != ' ') {
        dentry.name.push_back(dir_content[begin]);
        begin++;
    }
    begin++;
    //get inum
    while(begin < dir_content.size() && dir_content[begin] != ' ') {
        if(47 < dir_content[begin] && dir_content[begin] < 58) {
            dentry.inum = dentry.inum*10 + dir_content[begin] - 48;
        } else {
            return -1;
        }
        begin++;
    }
    if(dentry.name.size() == 0 || dentry.inum == 0)
        return -1;
    return begin + 1;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */


    if(!isdir(parent)) {
        found = false;
        return OK;
    } 

    std::string dir_content;
    yfs_client::dirent dentry;
    int begin = 0;

    printf("parent:%d\n", parent);
    ec->get((extent_protocol::extentid_t)parent, dir_content);

    printf("dir_content:%s\n", dir_content.c_str());
    while((begin = get_dentry(dir_content, begin, dentry)) >= 0 ) {
        if(!dentry.name.compare(name)) {
            found = true;
            ino_out = dentry.inum;
            return r;
        }
    }

    found = false;
    return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */

    if(!isdir(dir)) {
        return OK;
    } 

    std::string dir_content;
    yfs_client::dirent dentry;
    int begin = 0;
    ec->get((extent_protocol::extentid_t)dir, dir_content);

    printf("dir_content in readdir:%s\n", dir_content.c_str());

    while((begin = get_dentry(dir_content, begin, dentry)) >= 0 ) {
        list.push_back(dentry);
    }

    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;
    std::string file;

    /*
     * your lab2 code goes here.
     * note: read using ec->get().
     */

    fileinfo fin;
    getfile(ino, fin); 
    if( (r = ec->get(ino, file)) != yfs_client::OK)
        return r;

    printf("fin.size :%d\n", fin.size);
    if(fin.size <= off )
        return r;
    else {
      //  data.assign(file.substr(off, size));
        data.clear();
        size = size + off < fin.size ? size : fin.size - off;
        for(int i = 0;i < size;i++) {
            data.push_back(file[off+i]);
        }
    }

    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */

    std::string file;
    if((r = ec->get(ino, file)) != yfs_client::OK) {
        return r;
    }
    bytes_written = size;
 //   bytes_written = size <= strlen(data) ? size : strlen(data);
    fileinfo fin;
    getfile(ino, fin); 

    printf("fin.size:%d off:%d size:%d\n", fin.size, off,size);
    if(fin.size >= off + size) {
        for(int i = off;i < off + size;i++) {
            file[i] = data[i - off];
        }
    } else if(fin.size < off + size && fin.size >= off) {
        for(int i = off;i < fin.size;i++) {
            file[i] = data[i - off];
        }
        for(int i = fin.size;i < off + size;i++) {
            file.push_back(data[i - off]);
        }
    } else {
        for(int i = fin.size;i < off;i++) {
            file.push_back('\0');
        }
        for(int i = off;i < off + size;i++) {
            file.push_back(data[i - off]);
        }
    }
    if((r = ec->put(ino, file)) != yfs_client::OK) {
        bytes_written = 0;
        return r;
    }
    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */

    return r;
}

