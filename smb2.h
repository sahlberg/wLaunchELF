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

//---------------------------------------------------------------------------
//End of file: smb2.h
//---------------------------------------------------------------------------
