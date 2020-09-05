/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
//---------------------------------------------------------------------------
//File name:   smb2.c
//---------------------------------------------------------------------------
#include <debug.h>
#include <netman.h>
#include <ps2ip.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <launchelf.h>
#include <sbv_patches.h>
#include <smb2.h>

#include <smb2/libsmb2.h>

struct smb2fh *lf = NULL;
#define DBG(message) \
        smb2_write(smb2_shares->smb2, lf, message, strlen(message));
        
struct smb2_share *smb2_shares = NULL;

static void EthStatusCheckCb(s32 alarm_id, u16 time, void *common)
{
	iWakeupThread(*(int*)common);
}

static int WaitValidNetState(int (*checkingFunction)(void))
{
	int ThreadID, retry_cycles;

	// Wait for a valid network status;
	ThreadID = GetThreadId();
	for(retry_cycles = 0; checkingFunction() == 0; retry_cycles++)
	{	//Sleep for 1000ms.
		SetAlarm(1000 * 16, &EthStatusCheckCb, &ThreadID);
		SleepThread();

		if(retry_cycles >= 10)	//10s = 10*1000ms
			return -1;
	}

	return 0;
}

static int ethGetNetIFLinkStatus(void)
{
	return(NetManIoctl(NETMAN_NETIF_IOCTL_GET_LINK_STATUS, NULL, 0, NULL, 0) == NETMAN_NETIF_ETH_LINK_STATE_UP);
}

static int ethWaitValidNetIFLinkState(void)
{
	return WaitValidNetState(&ethGetNetIFLinkStatus);
}

static void free_smb2_share(struct smb2_share *smb2_share)
{
        if (smb2_share == NULL) {
                return;
        }
        free(smb2_share->name);
        free(smb2_share->user);
        free(smb2_share->password);
        free(smb2_share);
}
                            
static int loadSMB2CNF(char *path)
{
	unsigned char *RAM_p;
        char *CNF_p, *name, *value;
        struct smb2_share *smb2_share = NULL;

	if (!(RAM_p = preloadCNF(path)))
		return -1;
	CNF_p = RAM_p;
	while (get_CNF_string(&CNF_p, &name, &value)) {
                if (smb2_share == NULL) {
                        smb2_share = malloc(sizeof(struct smb2_share));
                        memset(smb2_share, 0, sizeof(struct smb2_share));
                }
                if (!strcmp(name, "NAME")) {
                        smb2_share->name = strdup(value);
                } else if (!strcmp(name, "USERNAME")) {
                        smb2_share->user = strdup(value);
                } else if (!strcmp(name, "PASSWORD")) {
                        smb2_share->password = strdup(value);
                } else if (!strcmp(name, "URL")) {
                        struct smb2_context *smb2;
                        struct smb2_url *url;

                        smb2_share->url = strdup(value);
                        smb2_share->smb2 = smb2 = smb2_init_context();
                        smb2_set_user(smb2, smb2_share->user);
                        smb2_set_password(smb2, smb2_share->password);
                        url = smb2_parse_url(smb2, smb2_share->url);
                        if (smb2_connect_share(smb2, url->server, url->share,
                                               smb2_share->user) < 0) {
                                scr_printf("failed to connect to : //%s/%s %s\n",
                                           url->server, url->share,
                                           smb2_get_error(smb2));
                                smb2_destroy_url(url);
                                smb2_destroy_context(smb2);
                                free_smb2_share(smb2_share);
                                smb2_share = NULL;
                                continue;
                        }
                        scr_printf("Mounted share: \\\\%s\\%s as %s\n",
                                   url->server, url->share, smb2_share->name);
                        smb2_destroy_url(url);
                        
                        smb2_share->next = smb2_shares;
                        smb2_shares = smb2_share;
                        smb2_share = NULL;
                }
        }
	free(RAM_p);
	return 0;
}

int init_smb2(const char *ip, const char *netmask, const char *gw)
{
	struct ip4_addr IP, NM, GW;
        int ip4[4];

	init_scr();
	{int ii; for (ii=0;ii<2;ii++)scr_printf("Woff\n");}

	NetManInit();

	sscanf(ip, "%d.%d.%d.%d", &ip4[0], &ip4[1], &ip4[2], &ip4[3]);
	IP4_ADDR(&IP, ip4[0], ip4[1], ip4[2], ip4[3]);
	sscanf(netmask, "%d.%d.%d.%d", &ip4[0], &ip4[1], &ip4[2], &ip4[3]);
	IP4_ADDR(&NM, ip4[0], ip4[1], ip4[2], ip4[3]);
	sscanf(gw, "%d.%d.%d.%d", &ip4[0], &ip4[1], &ip4[2], &ip4[3]);
	IP4_ADDR(&GW, ip4[0], ip4[1], ip4[2], ip4[3]);
	ps2ipInit(&IP, &NM, &GW);

	scr_printf("Waiting for connection...\n");
	if(ethWaitValidNetIFLinkState() != 0) {
		scr_printf("Error: failed to get valid link status.\n");
		return -1;
	}
	scr_printf("Network Initialized\n");
        
        loadSMB2CNF("mc0:/SYS-CONF/SMB2.CNF");
	scr_printf("init_smb2.\n");

        return 0;
}

static struct smb2_share *smb2_find_share(const char *name)
{
        struct smb2_share *share;

        for (share = smb2_shares; share; share = share->next) {
                if (!strcmp(share->name, name)) {
                        return share;
                }
        }

        return NULL;
}

static void set_time(sceMcStDateTime *ps2time, u64 smb2time)
{
        struct tm *tm;
        time_t t = smb2time & 0xffffffff;

        tm = localtime(&t);
        if (tm == NULL) {
                return;
        }
        ps2time->Sec   = tm->tm_sec;
        ps2time->Min   = tm->tm_min;
        ps2time->Hour  = tm->tm_hour;
        ps2time->Day   = tm->tm_mday + 1;
        ps2time->Month = tm->tm_mon + 1;
        ps2time->Year  = tm->tm_year + 1900;
}

static void find_share(const char *path, char **name, char **p,
                       struct smb2_share **share)
{
        *name = strdup(&path[6]);
        if (*name == NULL) {
                return;
        }
        *p = strchr(*name, '/');
        if (*p == NULL) {
                return;
        }
        *((*p)++) = 0;

	*share = smb2_find_share(*name);
        if (*share == NULL) {
                return;
        }
}

int readSMB2(const char *path, FILEINFO *info, int max)
{
        int n = 0;
        struct smb2dir *dir = NULL;
        struct smb2_share *share = NULL;
        char *name = NULL, *p;
        struct smb2dirent *ent;

        if (lf == NULL) {
                lf = smb2_open(smb2_shares->smb2, "LOG.txt", O_CREAT|O_WRONLY);
                DBG("Log started\n");
        }

        /* Root of smb2: is a list of all the named shares */
        if (path[6] == '\0') {
                for (share = smb2_shares; share; share = share->next) {
                        if (strlen(share->name) >= MAX_NAME) {
                                continue;
                        }
                        strncpy(info[n].name, share->name, MAX_NAME);
                        info[n].name[MAX_NAME] = 0;
                        clear_mcTable(&info[n].stats);
			info[n].stats.AttrFile = MC_ATTR_norm_folder;
                        strncpy((char *)info[n].stats.EntryName, info[n].name, 32);
                        n++;
                        if (n >= max) {
                                break;
                        }
                }
                goto finished;
        }

        /* Find the share */
        find_share(path, &name, &p, &share);
        if (share == NULL) {
                goto finished;
        }

	dir = smb2_opendir(share->smb2, p);
	if (dir == NULL) {
                goto finished;
        }
        while ((ent = smb2_readdir(share->smb2, dir))) {
                if (!strcmp(ent->name, ".") || !strcmp(ent->name, "..")) {
                        continue;
                }
                if (ent->st.smb2_type != SMB2_TYPE_FILE &&
                    ent->st.smb2_type != SMB2_TYPE_DIRECTORY) {
                        continue;
                }
                if (strlen(ent->name) >= MAX_NAME) {
                        continue;
                }
		strncpy(info[n].name, ent->name, MAX_NAME);
                info[n].name[MAX_NAME] = 0;
		clear_mcTable(&info[n].stats);
		if (ent->st.smb2_type == SMB2_TYPE_DIRECTORY) {
			info[n].stats.AttrFile = MC_ATTR_norm_folder;
		} else {
			info[n].stats.AttrFile = MC_ATTR_norm_file;
			info[n].stats.FileSizeByte = ent->st.smb2_size;
			info[n].stats.Reserve2 = 0;
		}

		strncpy((char *)info[n].stats.EntryName, info[n].name, 32);
		set_time(&info[n].stats._Create, ent->st.smb2_ctime);
		set_time(&info[n].stats._Modify, ent->st.smb2_mtime);
		n++;
		if (n >= max)
			break;
        }
	size_valid = 1;
	time_valid = 1;

 finished:
        free(name);
        if (dir) {
                smb2_closedir(share->smb2, dir);
        }
        return n;
}

int SMB2mkdir(const char *path, int fileMode)
{
        struct smb2_share *share = NULL;
        char *name = NULL, *p = NULL;
        int rc = 0;

        if (path[6] == '\0') {
                return -EINVAL;
        }

        find_share(path, &name, &p, &share);
        if (share == NULL) {
                free(name);
                return -EINVAL;
        }

        rc = smb2_mkdir(share->smb2, p);
        if (rc) {
                goto finished;
        }

finished:
        free(name);
        return rc;
}

//---------------------------------------------------------------------------
//End of file: smb2.c
//---------------------------------------------------------------------------
