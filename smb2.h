/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
//---------------------------------------------------------------------------
//File name:   smb2.h
//---------------------------------------------------------------------------
struct smb2_share {
        struct smb2_share *next;
        struct smb2_context *smb2;
        char *name;
        const char *user;
        const char *password;
        char *url;
};

extern struct smb2_share *smb2_shares;

int init_smb2(const char *ip, const char *netmask, const char *gw);
int readSMB2(const char *path, FILEINFO *info, int max);
int SMB2mkdir(const char *dir, int fileMode);
int SMB2rmdir(const char *dir);
int SMB2unlink(const char *dir);
int SMB2rename(const char *path, const char *newpath);

struct SMB2FH {
        struct smb2_context *smb2;
        struct smb2fh *fh;
};
struct SMB2FH *SMB2open(const char *path, int mode);
int SMB2close(struct SMB2FH *fh);
int SMB2read(struct SMB2FH *fh, char *buf, int size);
int SMB2write(struct SMB2FH *fh, char *buf, int size);
int SMB2lseek(struct SMB2FH *fh, int where, int how);

//---------------------------------------------------------------------------
//End of file: smb2.h
//---------------------------------------------------------------------------
