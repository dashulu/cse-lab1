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

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
//  lc = new lock_client(lock_dst);
  lc = new lock_client_cache(lock_dst);
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

    YFSScopedLock sl(lc, inum);

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

    YFSScopedLock sl(lc, inum);

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

    YFSScopedLock sl(lc, inum);

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

    extent_protocol::attr a;
    std::string buf;

    YFSScopedLock sl(lc, ino);

    if (ec->getattr(ino, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    if (size == a.size)
        goto release;
    if (ec->get(ino, buf) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    if (size < a.size) {
        buf = buf.substr(0, size);
        assert(buf.size() == size);
        ec->put(ino, buf);
    }
    else {
        buf.resize(size, '\0');
        assert(buf.size() == size);
        ec->put(ino, buf);
    }

release:
    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out, bool _isdir)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    bool found;

    YFSScopedLock sl(lc, parent);

    if ((r = _lookup(parent, name, found, ino_out)) != OK)
        return r;
    if (found)
        return EXIST;

    // create new inode entry
    if (_isdir) {
        ec->create(extent_protocol::T_DIR, ino_out);
        assert(isdir(ino_out));
    }
    else {
        ec->create(extent_protocol::T_FILE, ino_out);
        assert(isfile(ino_out));
    }

    // add <name, inum> to parent
    std::string buf;
    std::stringstream sst;
    if (ec->get(parent, buf) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    sst << name << " " << ino_out << "\n";
    buf.append(sst.str());
    if (ec->put(parent, buf) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

release:
    return r;
}

int
yfs_client::_lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    std::string buf;
    if (ec->get(parent, buf) != extent_protocol::OK) {
        r = IOERR;
        return r;
    }

    std::istringstream ist(buf);
    std::string target(name), e_name;
    inum e_ino;
    found = false;
    while (ist >> e_name && ist >> e_ino) {
        if (e_name.compare(target) == 0) {
            found = true;
            ino_out = e_ino;
            break;
        }
    }

    return r;
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
    YFSScopedLock sl(lc, parent);
    std::string buf;
    if (ec->get(parent, buf) != extent_protocol::OK) {
        r = IOERR;
        return r;
    }

    std::istringstream ist(buf);
    std::string target(name), e_name;
    inum e_ino;
    found = false;
    while (ist >> e_name && ist >> e_ino) {
        if (e_name.compare(target) == 0) {
            found = true;
            ino_out = e_ino;
            break;
        }
    }

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

    YFSScopedLock sl(lc, dir);

    std::string buf;
    if (ec->get(dir, buf) != extent_protocol::OK) {
        r = IOERR;
        return r;
    }

    std::istringstream ist(buf);
    std::string e_name;
    inum e_ino;
    while (ist >> e_name && ist >> e_ino) {
        struct dirent d = {e_name, e_ino};
        list.push_back(d);
    }
    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: read using ec->get().
     */
    std::string buf;
    extent_protocol::attr a;

    YFSScopedLock sl(lc, ino);

    if (ec->getattr(ino, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    if (ec->get(ino, buf) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    if (off >= a.size) {
        data = "";
        assert(data.size() == 0);
    }
    if (off + size > a.size) {
        data = buf.substr(off, a.size - off);
        assert(data.size() == (size_t)a.size - off);
    } else {
        data = buf.substr(off, size);
        assert(data.size() == size);
    }

release:
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
    std::string buf;
    std::string head, tail;
    extent_protocol::attr a;

    YFSScopedLock sl(lc, ino);

    if (ec->getattr(ino, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    if (ec->get(ino, buf) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    if (off > a.size) {
        bytes_written = off - a.size + size;
        size_t original_len = buf.size();
        assert(original_len == a.size);
        buf.resize(off, '\0');
        size_t new_len = buf.size();
        assert(new_len == (size_t)off);
        ec->put(ino, buf.append(data, size));
    } else {
        bytes_written = a.size - off + size;
        head = buf.substr(0, off);
        head.append(data, size);
        if (off + size < a.size) {
            tail = buf.substr(off + size, a.size - (off + size));
            head.append(tail);
        }
        if (a.size > off + size)
            assert(head.size() == a.size);
        else
            assert(head.size() == off + size);

        ec->put(ino, head);
    }

release:
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

    std::string buf, new_buf;

    YFSScopedLock sl(lc, parent);
    
    if (ec->get(parent, buf) != extent_protocol::OK)
        return IOERR;

    std::istringstream ist(buf), new_ist;
    std::string target(name), e_name;
    inum e_ino;
    bool found = false;
    while (ist >> e_name && ist >> e_ino) {
        if (e_name.compare(target) == 0) {
            found = true;
            ec->remove(e_ino);
        } else {
            std::stringstream sst;
            sst << e_name << " " << e_ino << "\n";
            new_buf.append(sst.str());
        }
    }

    if (found) {
        if (ec->put(parent, new_buf) != extent_protocol::OK)
            return IOERR;
        else
            return r;
    } else
        return EXIST;
}
