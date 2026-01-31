#pragma once

static char* get_access_string(mode_t mode, char* str)
{
    switch (mode & S_IFMT)
    {
    case S_IFDIR:  str[0] = 'd'; break;
    case S_IFCHR:  str[0] = 'c'; break;
    case S_IFBLK:  str[0] = 'b'; break;
    case S_IFLNK:  str[0] = 'l'; break;
    case S_IFIFO:  str[0] = 'p'; break;
    case S_IFSOCK: str[0] = 's'; break;
    default:       str[0] = '-'; break;
    }

    str[1] = (mode & S_IRUSR) ? 'r' : '-';
    str[2] = (mode & S_IWUSR) ? 'w' : '-';
    if (mode & S_ISUID)
        str[3] = (mode & S_IXUSR) ? 's' : 'S';
    else
        str[3] = (mode & S_IXUSR) ? 'x' : '-';

    str[4] = (mode & S_IRGRP) ? 'r' : '-';
    str[5] = (mode & S_IWGRP) ? 'w' : '-';
    if (mode & S_ISGID)
        str[6] = (mode & S_IXGRP) ? 's' : 'S';
    else
        str[6] = (mode & S_IXGRP) ? 'x' : '-';

    str[7] = (mode & S_IROTH) ? 'r' : '-';
    str[8] = (mode & S_IWOTH) ? 'w' : '-';
    if (mode & S_ISVTX)
        str[9] = (mode & S_IXOTH) ? 't' : 'T';
    else
        str[9] = (mode & S_IXOTH) ? 'x' : '-';

    str[10] = 0;
    return str;
}