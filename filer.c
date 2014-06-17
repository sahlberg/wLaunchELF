#include "launchelf.h"

typedef struct{
	char name[256];
	char title[16*4+1];
	unsigned short attr;
} FILEINFO;

enum
{
	COPY,
	CUT,
	PASTE,
	DELETE,
	RENAME,
	NEWDIR,
	GETSIZE,
	NUM_MENU
};

unsigned char *elisaFnt=NULL;
size_t freeSpace;
int mcfreeSpace;
int vfreeSpace;
int cut;
int nclipFiles, nmarks, nparties;
int title;
char mountedParty[2][MAX_NAME];
char parties[MAX_PARTITIONS][MAX_NAME];
char clipPath[MAX_PATH], LastDir[MAX_NAME], marks[MAX_ENTRY];
FILEINFO clipFiles[MAX_ENTRY];
int fileMode =  FIO_S_IRUSR | FIO_S_IWUSR | FIO_S_IXUSR | FIO_S_IRGRP | FIO_S_IWGRP | FIO_S_IXGRP | FIO_S_IROTH | FIO_S_IWOTH | FIO_S_IXOTH;

///////////////////////////////////////////////////////////////////////////
// HDD�̃p�[�e�B�V�����ƃp�X���擾
int getHddParty(const char *path, const FILEINFO *file, char *party, char *dir)
{
	char fullpath[MAX_PATH], *p;
	
	if(strncmp(path,"hdd",3)) return -1;
	
	strcpy(fullpath, path);
	if(file!=NULL){
		strcat(fullpath, file->name);
		if(file->attr & FIO_S_IFDIR) strcat(fullpath,"/");
	}
	if((p=strchr(&fullpath[6], '/'))==NULL) return -1;
	if(dir!=NULL) sprintf(dir, "pfs0:%s", p);
	*p=0;
	if(party!=NULL) sprintf(party, "hdd0:%s", &fullpath[6]);
	
	return 0;
}

///////////////////////////////////////////////////////////////////////////
// �p�[�e�B�V�����̃}�E���g
int mountParty(const char *party)
{
	if(!strcmp(party, mountedParty[0]))
		return 0;
	else if(!strcmp(party, mountedParty[1]))
		return 1;
	
	fileXioUmount("pfs0:"); mountedParty[0][0]=0;
	if(fileXioMount("pfs0:", party, FIO_MT_RDWR) < 0) return -1;
	strcpy(mountedParty[0], party);
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// �m�F�_�C�A���O
int ynDialog(const char *message)
{
	char msg[512];
	int dh, dw, dx, dy;
	int sel=0, a=6, b=4, c=2, n, tw;
	int i, x, len, ret;
	
	strcpy(msg, message);
	
	for(i=0,n=1; msg[i]!=0; i++){
		if(msg[i]=='\n'){
			msg[i]=0;
			n++;
		}
	}
	for(i=len=tw=0; i<n; i++){
		ret = printXY(&msg[len], 0, 0, 0, FALSE);
		if(ret>tw) tw=ret;
		len=strlen(&msg[len])+1;
	}
	if(tw<96) tw=96;
	
	dh = 16*(n+1)+2*2+a+b+c;
	dw = 2*2+a*2+tw;
	dx = (512-dw)/2;
	dy = (432-dh)/2;
	printf("tw=%d\ndh=%d\ndw=%d\ndx=%d\ndy=%d\n", tw,dh,dw,dx,dy);
	
	while(1){
		waitPadReady(0, 0);
		if(readpad()){
			if(new_pad & PAD_LEFT){
				sel=0;
			}else if(new_pad & PAD_RIGHT){
				sel=1;
			}else if(new_pad & PAD_CROSS){
				ret=-1;
				break;
			}else if(new_pad & PAD_CIRCLE){
				if(sel==0) ret=1;
				else	   ret=-1;
				break;
			}
		}
		
		itoSprite(setting->color[0], 0, (SCREEN_MARGIN+FONT_HEIGHT+4)/2,
		SCREEN_WIDTH, (SCREEN_MARGIN+FONT_HEIGHT+4+FONT_HEIGHT)/2, 0);
		itoSprite(setting->color[0], dx-2, (dy-2)/2, dx+dw+2, (dy+dh+4)/2, 0);
		drawFrame(dx, dy/2, dx+dw, (dy+dh)/2, setting->color[1]);
		for(i=len=0; i<n; i++){
			printXY(&msg[len], dx+2+a,(dy+a+2+i*16)/2, setting->color[3],TRUE);
			len=strlen(&msg[len])+1;
		}
		x=(tw-96)/2;
		printXY(" OK   CANCEL", dx+a+x, (dy+a+b+2+n*16)/2, setting->color[3],TRUE);
		if(sel==0)
			drawChar(127, dx+a+x,(dy+a+b+2+n*16)/2, setting->color[3]);
		else
			drawChar(127,dx+a+x+40,(dy+a+b+2+n*16)/2,setting->color[3]);
		drawScr();
	}
	x=(tw-96)/2;
	drawChar(' ', dx+a+x,(dy+a+b+2+n*16)/2, setting->color[3]);
	drawChar(' ',dx+a+x+40,(dy+a+b+2+n*16)/2,setting->color[3]);
	return ret;
}

////////////////////////////////////////////////////////////////////////
// �N�C�b�N�\�[�g
int cmpFile(FILEINFO *a, FILEINFO *b)
{
	unsigned char *p, ca, cb;
	int i, n, ret, aElf=FALSE, bElf=FALSE, t=title;
	
	if(a->attr==b->attr){
		if(a->attr & FIO_S_IFREG){
			p = strrchr(a->name, '.');
			if(p!=NULL && !stricmp(p+1, "ELF")) aElf=TRUE;
			p = strrchr(b->name, '.');
			if(p!=NULL && !stricmp(p+1, "ELF")) bElf=TRUE;
			if(aElf && !bElf)		return -1;
			else if(!aElf && bElf)	return 1;
			t=FALSE;
		}
		if(t){
			if(a->title[0]!=0 && b->title[0]==0) return -1;
			else if(a->title[0]==0 && b->title[0]!=0) return 1;
			else if(a->title[0]==0 && b->title[0]==0) t=FALSE;
		}
		if(t) n=strlen(a->title);
		else  n=strlen(a->name);
		for(i=0; i<=n; i++){
			if(t){
				ca=a->title[i]; cb=b->title[i];
			}else{
				ca=a->name[i]; cb=b->name[i];
				if(ca>='a' && ca<='z') ca-=0x20;
				if(cb>='a' && cb<='z') cb-=0x20;
			}
			ret = ca-cb;
			if(ret!=0) return ret;
		}
		return 0;
	}
	
	if(a->attr & FIO_S_IFDIR)	return -1;
	else						return 1;
}
void sort(FILEINFO *a, int left, int right) {
	FILEINFO tmp, pivot;
	int i, p;
	
	if (left < right) {
		pivot = a[left];
		p = left;
		for (i=left+1; i<=right; i++) {
			if (cmpFile(&a[i],&pivot)<0){
				p=p+1;
				tmp=a[p];
				a[p]=a[i];
				a[i]=tmp;
			}
		}
		a[left] = a[p];
		a[p] = pivot;
		sort(a, left, p-1);
		sort(a, p+1, right);
	}
}

////////////////////////////////////////////////////////////////////////
// �������[�J�[�h�ǂݍ���
int readMC(const char *path, FILEINFO *info, int max)
{
	static mcTable mcDir[MAX_ENTRY] __attribute__((aligned(64)));
	char dir[MAX_PATH];
	int i, j, ret;
	
	mcSync(0,NULL,NULL);
	
	strcpy(dir, &path[4]); strcat(dir, "*");
	mcGetDir(path[2]-'0', 0, dir, 0, MAX_ENTRY-2, mcDir);
	mcSync(0, NULL, &ret);
	
	for(i=j=0; i<ret; i++)
	{
		if(mcDir[i].attrFile & MC_ATTR_SUBDIR &&
		(!strcmp(mcDir[i].name,".") || !strcmp(mcDir[i].name,"..")))
			continue;
		strcpy(info[j].name, mcDir[i].name);
		if(mcDir[i].attrFile & MC_ATTR_SUBDIR)
			info[j].attr = FIO_S_IFDIR;
		else
			info[j].attr = FIO_S_IFREG;
		j++;
	}
	
	return j;
}

////////////////////////////////////////////////////////////////////////
// CD�ǂݍ���
int readCD(const char *path, FILEINFO *info, int max)
{
	static struct TocEntry TocEntryList[MAX_ENTRY];
	char dir[MAX_PATH];
	int i, j, n;
	
	loadCdModules();
	
	strcpy(dir, &path[5]);
	CDVD_FlushCache();
	n = CDVD_GetDir(dir, NULL, CDVD_GET_FILES_AND_DIRS, TocEntryList, MAX_ENTRY, dir);
	
	for(i=j=0; i<n; i++)
	{
		if(TocEntryList[i].fileProperties & 0x02 &&
		 (!strcmp(TocEntryList[i].filename,".") ||
		  !strcmp(TocEntryList[i].filename,"..")))
			continue;
		strcpy(info[j].name, TocEntryList[i].filename);
		if(TocEntryList[i].fileProperties & 0x02)
			info[j].attr = FIO_S_IFDIR;
		else
			info[j].attr = FIO_S_IFREG;
		j++;
	}
	
	return j;
}

////////////////////////////////////////////////////////////////////////
// �p�[�e�B�V�������X�g�ݒ�
void setPartyList(void)
{
	iox_dirent_t dirEnt;
	int hddFd;
	
	nparties=0;
	
	if((hddFd=fileXioDopen("hdd0:")) < 0)
		return;
	while(fileXioDread(hddFd, &dirEnt) > 0)
	{
		if(nparties >= MAX_PARTITIONS)
			break;
		if((dirEnt.stat.attr & ATTR_SUB_PARTITION) 
				|| (dirEnt.stat.mode == FS_TYPE_EMPTY))
			continue;
		if(!strncmp(dirEnt.name, "PP.HDL.", 7))
			continue;
		if(!strncmp(dirEnt.name, "__", 2) && strcmp(dirEnt.name, "__boot"))
			continue;
		
		strcpy(parties[nparties++], dirEnt.name);
	}
	fileXioDclose(hddFd);
}

////////////////////////////////////////////////////////////////////////
// HDD�ǂݍ���
int readHDD(const char *path, FILEINFO *info, int max)
{
	iox_dirent_t dirbuf;
	char party[MAX_PATH], dir[MAX_PATH];
	int i=0, fd, ret;
	
	if(nparties==0){
		loadHddModules();
		setPartyList();
	}
	
	if(!strcmp(path, "hdd0:/")){
		for(i=0; i<nparties; i++){
			strcpy(info[i].name, parties[i]);
			info[i].attr = FIO_S_IFDIR;
		}
		return nparties;
	}
	
	getHddParty(path,NULL,party,dir);
	ret = mountParty(party);
	if(ret<0) return 0;
	dir[3] = ret+'0';
	
	if((fd=fileXioDopen(dir)) < 0) return 0;
	
	while(fileXioDread(fd, &dirbuf)){
		if(dirbuf.stat.mode & FIO_S_IFDIR &&
		(!strcmp(dirbuf.name,".") || !strcmp(dirbuf.name,"..")))
			continue;
		
		info[i].attr = dirbuf.stat.mode;
		strcpy(info[i].name, dirbuf.name);
		i++;
		if(i==max) break;
	}
	
	fileXioDclose(fd);
	
	return i;
}

////////////////////////////////////////////////////////////////////////
// USB�}�X�X�g���[�W�ǂݍ���
int readMASS(const char *path, FILEINFO *info, int max)
{
	fat_dir_record record;
	int ret, n=0;
	
	loadUsbModules();
	
	ret = usb_mass_getFirstDirentry((char*)path+5, &record);
	while(ret > 0){
		if(record.attr & 0x10 &&
		(!strcmp(record.name,".") || !strcmp(record.name,".."))){
			ret = usb_mass_getNextDirentry(&record);
			continue;
		}
		
		strcpy(info[n].name, record.name);
		if(record.attr & 0x10)
			info[n].attr = FIO_S_IFDIR;
		else
			info[n].attr = FIO_S_IFREG;
		n++;
		ret = usb_mass_getNextDirentry(&record);
	}
	
	return n;
}

////////////////////////////////////////////////////////////////////////
// �t�@�C�����X�g�擾
int getDir(const char *path, FILEINFO *info)
{
	int max=MAX_ENTRY-2;
	int n;
	
	if(!strncmp(path, "mc", 2))			n=readMC(path, info, max);
	else if(!strncmp(path, "hdd", 3))	n=readHDD(path, info, max);
	else if(!strncmp(path, "mass", 4))	n=readMASS(path, info, max);
	else if(!strncmp(path, "cdfs", 4))	n=readCD(path, info, max);
	else return 0;
	
	return n;
}

///////////////////////////////////////////////////////////////////////////
// �Z�[�u�f�[�^�^�C�g���̎擾
int getGameTitle(const char *path, const FILEINFO *file, char *out)
{
	iox_dirent_t dirEnt;
	char party[MAX_NAME], dir[MAX_PATH];
	int fd=-1, dirfd=-1, size, hddin=FALSE, ret;
	
	if(file->attr & FIO_S_IFREG) return -1;
	if(path[0]==0 || !strcmp(path,"hdd0:/")) return -1;
	
	if(!strncmp(path, "hdd", 3)){
		ret = getHddParty(path, file, party, dir);
		if(mountParty(party)<0) return -1;
		dir[3]=ret+'0';
		hddin=TRUE;
	}else
		sprintf(dir, "%s%s/", path, file->name);
	
	ret = -1;
	if(hddin){
		if((dirfd=fileXioDopen(dir)) < 0) goto error;
		while(fileXioDread(dirfd, &dirEnt)){
			if(dirEnt.stat.mode & FIO_S_IFREG &&
			 !strcmp(dirEnt.name,"icon.sys")){
				strcat(dir, "icon.sys");
				if((fd=fileXioOpen(dir, O_RDONLY, fileMode)) < 0)
					goto error;
				if((size=fileXioLseek(fd,0,SEEK_END)) <= 0x100)
					goto error;
				fileXioLseek(fd,0xC0,SEEK_SET);
				fileXioRead(fd, out, 16*4);
				out[16*4] = 0;
				fileXioClose(fd); fd=-1;
				ret=0;
				break;
			}
		}
		fileXioDclose(dirfd); dirfd=-1;
	}else{
		strcat(dir, "icon.sys");
		if((fd=fioOpen(dir, O_RDONLY)) < 0) goto error;
		if((size=fioLseek(fd,0,SEEK_END)) <= 0x100) goto error;
		fioLseek(fd,0xC0,SEEK_SET);
		fioRead(fd, out, 16*4);
		out[16*4] = 0;
		fioClose(fd); fd=-1;
		ret=0;
	}
error:
	if(fd>=0){
		if(hddin) fileXioClose(fd);
		else	  fioClose(fd);
	}
	if(dirfd>=0) fileXioDclose(dirfd);
	return ret;
}

////////////////////////////////////////////////////////////////////////
// ���j���[
int menu(const char *path, const char *file)
{
	uint64 color;
	char enable[NUM_MENU], tmp[64];
	int x, y, i, sel;
	
	// ���j���[���ڗL���E�����ݒ�
	memset(enable, TRUE, NUM_MENU);
	if(!strcmp(path,"hdd0:/") || path[0]==0){
		enable[COPY] = FALSE;
		enable[CUT] = FALSE;
		enable[PASTE] = FALSE;
		enable[DELETE] = FALSE;
		enable[RENAME] = FALSE;
		enable[NEWDIR] = FALSE;
		enable[GETSIZE] = FALSE;
	}
	if(!strncmp(path,"cdfs",4)){
		enable[CUT] = FALSE;
		enable[PASTE] = FALSE;
		enable[DELETE] = FALSE;
		enable[RENAME] = FALSE;
		enable[NEWDIR] = FALSE;
	}
	if(!strncmp(path, "mass", 4)){
		//enable[CUT] = FALSE;
		//enable[PASTE] = FALSE;
		//enable[DELETE] = FALSE;
		enable[RENAME] = FALSE;
		//enable[NEWDIR] = FALSE;
	}
	if(!strncmp(path, "mc", 2))
		enable[RENAME] = FALSE;
	if(nmarks==0){
		if(!strcmp(file, "..")){
			enable[COPY] = FALSE;
			enable[CUT] = FALSE;
			enable[DELETE] = FALSE;
			enable[RENAME] = FALSE;
			enable[GETSIZE] = FALSE;
		}
	}else
		enable[RENAME] = FALSE;
	if(nclipFiles==0)
		enable[PASTE] = FALSE;
	// �����I�����ݒ�
	for(sel=0; sel<NUM_MENU; sel++)
		if(enable[sel]==TRUE) break;
	
	while(1){
		waitPadReady(0, 0);
		if(readpad()){
			if(new_pad & PAD_UP && sel<NUM_MENU){
				do{
					sel--;
					if(sel<0) sel=NUM_MENU-1;
				}while(!enable[sel]);
			}else if(new_pad & PAD_DOWN && sel<NUM_MENU){
				do{
					sel++;
					if(sel==NUM_MENU) sel=0;
				}while(!enable[sel]);
			}else if(new_pad & PAD_CROSS)
				return -1;
			else if(new_pad & PAD_CIRCLE)
				break;
		}
		
		// �`��J�n
		itoSprite(setting->color[0], 401, 32, 493, 91+8, 0);
		drawFrame(402, 33, 490, 89+8, setting->color[1]);
		
		for(i=0,y=74; i<NUM_MENU; i++){
			if(i==COPY)			strcpy(tmp, "Copy");
			else if(i==CUT)		strcpy(tmp, "Cut");
			else if(i==PASTE)	strcpy(tmp, "Paste");
			else if(i==DELETE)	strcpy(tmp, "Delete");
			else if(i==RENAME)	strcpy(tmp, "Rename");
			else if(i==NEWDIR)	strcpy(tmp, "New Dir");
			else if(i==GETSIZE) strcpy(tmp, "Get Size");
			
			if(enable[i])	color = setting->color[3];
			else			color = setting->color[1];
			
			printXY(tmp, 418, y/2, color, TRUE);
			y+=FONT_HEIGHT;
		}
		if(sel<NUM_MENU)
			drawChar(127, 410, (74+sel*FONT_HEIGHT)/2, setting->color[3]);
		
		// �������
		x = SCREEN_MARGIN;
		y = SCREEN_HEIGHT-SCREEN_MARGIN-FONT_HEIGHT;
		itoSprite(setting->color[0],
			0,
			y/2,
			SCREEN_WIDTH,
			y/2+8, 0);
		printXY("��:OK �~:Cancel", x, y/2, setting->color[2], TRUE);
		drawScr();
	}
	
	return sel;
}

//////////////////////////////////////////////////////////////////////////
// �t�@�C���T�C�Y�擾
size_t getFileSize(const char *path, const FILEINFO *file)
{
	size_t size;
	FILEINFO files[MAX_ENTRY];
	char dir[MAX_PATH], party[MAX_NAME];
	int nfiles, i, ret, fd;
	
	if(file->attr & FIO_S_IFDIR){
		sprintf(dir, "%s%s/", path, file->name);
		// �Ώۃt�H���_���̑S�t�@�C���E�t�H���_�T�C�Y�����v
		nfiles = getDir(dir, files);
		for(i=size=0; i<nfiles; i++){
			ret=getFileSize(dir, &files[i]);
			if(ret < 0) size = -1;
			else		size+=ret;
		}
	} else {
		// �p�[�e�B�V�����}�E���g
		if(!strncmp(path, "hdd", 3)){
			getHddParty(path,file,party,dir);
			ret = mountParty(party);
			if(ret<0) return 0;
			dir[3] = ret+'0';
		}else
			sprintf(dir, "%s%s", path, file->name);
		// �t�@�C���T�C�Y�擾
		if(!strncmp(path, "hdd", 3)){
			fd = fileXioOpen(dir, O_RDONLY, fileMode);
			size = fileXioLseek(fd,0,SEEK_END);
			fileXioClose(fd);
		}else{
			fd = fioOpen(dir, O_RDONLY);
			size = fioLseek(fd,0,SEEK_END);
			fioClose(fd);;
		}
	}
	return size;
}

////////////////////////////////////////////////////////////////////////
// �t�@�C���E�t�H���_�폜
int delete(const char *path, const FILEINFO *file)
{
	FILEINFO files[MAX_ENTRY];
	char party[MAX_NAME], dir[MAX_PATH], hdddir[MAX_PATH];
	int nfiles, i, ret;
	
	// �p�[�e�B�V�����}�E���g
	if(!strncmp(path, "hdd", 3)){
		getHddParty(path,file,party,hdddir);
		ret = mountParty(party);
		if(ret<0) return 0;
		hdddir[3] = ret+'0';
	}
	sprintf(dir, "%s%s", path, file->name);
	
	if(file->attr & FIO_S_IFDIR){
		strcat(dir,"/");
		// �Ώۃt�H���_���̑S�t�@�C���E�t�H���_���폜
		nfiles = getDir(dir, files);
		for(i=0; i<nfiles; i++){
			ret=delete(dir, &files[i]);
			if(ret < 0) return -1;
		}
		// �Ώۃt�H���_���폜
		if(!strncmp(dir, "mc", 2)){
			mcSync(0,NULL,NULL);
			mcDelete(dir[2]-'0', 0, &dir[4]);
			mcSync(0, NULL, &ret);
		}else if(!strncmp(path, "hdd", 3)){
			ret = fileXioRmdir(hdddir);
		}else if(!strncmp(path, "mass", 4)){
			sprintf(dir, "mass0:%s%s", &path[5], file->name);
			ret = fioRmdir(dir);
			if (ret < 0){
				dir[4] = 1 + '0';
				ret = fioRmdir(dir);
			}
		}
	} else {
		// �Ώۃt�@�C�����폜
		if(!strncmp(path, "mc", 2)){
			mcSync(0,NULL,NULL);
			mcDelete(dir[2]-'0', 0, &dir[4]);
			mcSync(0, NULL, &ret);
		}else if(!strncmp(path, "hdd", 3)){
			ret = fileXioRemove(hdddir);
		}else if(!strncmp(path, "mass", 4)){
			ret = fioRemove(dir);
		}
	}
	return ret;
}

////////////////////////////////////////////////////////////////////////
// �t�@�C���E�t�H���_���l�[��
int Rename(const char *path, const FILEINFO *file, const char *name)
{
	char party[MAX_NAME], oldPath[MAX_PATH], newPath[MAX_PATH];
	int ret=0;
	
	if(!strncmp(path, "hdd", 3)){
		sprintf(party, "hdd0:%s", &path[6]);
		*strchr(party, '/')=0;
		sprintf(oldPath, "pfs0:%s", strchr(&path[6], '/')+1);
		sprintf(newPath, "%s%s", oldPath, name);
		strcat(oldPath, file->name);
		
		ret = mountParty(party);
		if(ret<0) return -1;
		oldPath[3] = newPath[3] = ret+'0';
		
		ret=fileXioRename(oldPath, newPath);
	}else
		return -1;
	
	return ret;
}

////////////////////////////////////////////////////////////////////////
// �V�K�t�H���_�쐬
int newdir(const char *path, const char *name)
{
	char party[MAX_NAME], dir[MAX_PATH];
	int ret=0;
	
	if(!strncmp(path, "hdd", 3)){
		getHddParty(path,NULL,party,dir);
		ret = mountParty(party);
		if(ret<0) return -1;
		dir[3] = ret+'0';
		//fileXioChdir(dir);
		strcat(dir, name);
		ret = fileXioMkdir(dir, fileMode);
	}else if(!strncmp(path, "mc", 2)){
		sprintf(dir, "%s%s", path+4, name);
		mcSync(0,NULL,NULL);
		mcMkDir(path[2]-'0', 0, dir);
		mcSync(0, NULL, &ret);
		if(ret == -4)
			ret = -17;
	}else if(!strncmp(path, "mass", 4)){
		strcpy(dir, path);
		strcat(dir, name);
		ret = fioMkdir(dir);
	}
	
	return ret;
}

////////////////////////////////////////////////////////////////////////
// �t�@�C���R�s�[
int copy(const char *outPath, const char *inPath, FILEINFO file, int n)
{
	FILEINFO files[MAX_ENTRY];
	char out[MAX_PATH], in[MAX_PATH], tmp[MAX_PATH],
		*buff=NULL, inParty[MAX_NAME], outParty[MAX_NAME];
	int hddout=FALSE, hddin=FALSE, nfiles, i;
	size_t size, outsize;
	int ret=-1, pfsout=-1, pfsin=-1, in_fd=-1, out_fd=-1, buffSize;
	
	sprintf(out, "%s%s", outPath, file.name);
	sprintf(in, "%s%s", inPath, file.name);
	
	// ���o�̓p�X�̐ݒ�ƃp�[�e�B�V�����̃}�E���g�B
	if(!strncmp(inPath, "hdd", 3)){
		hddin = TRUE;
		getHddParty(inPath, &file, inParty, in);
		if(!strcmp(inParty, mountedParty[0]))
			pfsin=0;
		else if(!strcmp(inParty, mountedParty[1]))
			pfsin=1;
		else
			pfsin=-1;
	}
	if(!strncmp(outPath, "hdd", 3)){
		hddout = TRUE;
		getHddParty(outPath, &file, outParty, out);
		if(!strcmp(outParty, mountedParty[0]))
			pfsout=0;
		else if(!strcmp(outParty, mountedParty[1]))
			pfsout=1;
		else
			pfsout=-1;
	}
	if(hddin){
		if(pfsin<0){
			if(pfsout==0) pfsin=1;
			else		  pfsin=0;
			sprintf(tmp, "pfs%d:", pfsin);
			if(mountedParty[pfsin][0]!=0)
				fileXioUmount(tmp); mountedParty[pfsin][0]=0;
			printf("%s mounting\n", inParty);
			if(fileXioMount(tmp, inParty, FIO_MT_RDWR) < 0) return -1;
			strcpy(mountedParty[pfsin], inParty);
		}
		in[3]=pfsin+'0';
	}else
		sprintf(in, "%s%s", inPath, file.name);
	if(hddout){
		if(pfsout<0){
			if(pfsin==0) pfsout=1;
			else		 pfsout=0;
			sprintf(tmp, "pfs%d:", pfsout);
			if(mountedParty[pfsout][0]!=0)
				fileXioUmount(tmp); mountedParty[pfsout][0]=0;
			if(fileXioMount(tmp, outParty, FIO_MT_RDWR) < 0) return -1;
			printf("%s mounting\n", outParty);
			strcpy(mountedParty[pfsout], outParty);
		}
		out[3]=pfsout+'0';
	}else
		sprintf(out, "%s%s", outPath, file.name);
	
	// �t�H���_�̏ꍇ
	if(file.attr & FIO_S_IFDIR){
		// �t�H���_�쐬
		ret = newdir(outPath, file.name);
		if(ret == -17){
			ret=-1;
			if(title) ret=getGameTitle(outPath, &file, tmp);
			if(ret<0) sprintf(tmp, "%s%s/", outPath, file.name);
			strcat(tmp, "\nOverwrite?");
			if(ynDialog(tmp)<0) return -1;
			drawMsg("Pasting...");
		} else if(ret < 0)
			return -1;
		// �t�H���_�̒��g��S�R�s�[
		sprintf(out, "%s%s/", outPath, file.name);
		sprintf(in, "%s%s/", inPath, file.name);
		nfiles = getDir(in, files);
		for(i=0; i<nfiles; i++)
			if(copy(out, in, files[i], n+1) < 0) return -1;
		return 0;
	}

	// �t�@�C���T�C�Y�擾
	if(hddin){
		in_fd = fileXioOpen(in, O_RDONLY, fileMode);
		if(in_fd<0) goto error;
		size = fileXioLseek(in_fd,0,SEEK_END);
		fileXioLseek(in_fd,0,SEEK_SET);
	}else{
		in_fd = fioOpen(in, O_RDONLY);
		if(in_fd<0) goto error;
		size = fioLseek(in_fd,0,SEEK_END);
		fioLseek(in_fd,0,SEEK_SET);
	}
	// �������Ɉ�x�œǂݍ��߂�t�@�C���T�C�Y�������ꍇ
	buff = (char*)malloc(size);
	if(buff==NULL){
		buff = (char*)malloc(32768);
		buffSize = 32768;
	}else
		buffSize = size;
	while(size>0){
		// ����
		if(hddin) buffSize = fileXioRead(in_fd, buff, buffSize);
		else	  buffSize = fioRead(in_fd, buff, buffSize);
		// �o��
		if(hddout){
			if(out_fd<0){
				// O_TRUNC �������Ȃ����߁A�I�[�v���O�Ƀt�@�C���폜
				fileXioRemove(out);
				
				out_fd = fileXioOpen(out,O_WRONLY|O_TRUNC|O_CREAT,fileMode);
				if(out_fd<0) goto error;
			}
			outsize = fileXioWrite(out_fd,buff,buffSize);
			if(buffSize!=outsize){
				fileXioClose(out_fd); out_fd=-1;
				fileXioRemove(out);
				goto error;
			}
		}else{
			if(out_fd<0){
				out_fd=fioOpen(out, O_WRONLY | O_TRUNC | O_CREAT);
				if(out_fd<0) goto error;
			}
			outsize = fioWrite(out_fd,buff,buffSize);
			if(buffSize!=outsize){
				fioClose(out_fd); out_fd=-1;
				mcSync(0,NULL,NULL);
				mcDelete(out[2]-'0', 0, &out[4]);
				mcSync(0, NULL, NULL);
				goto error;
			}
		}
		size -= buffSize;
	}
	ret=0;
error:
	free(buff);
	if(in_fd>0){
		if(hddin) fileXioClose(in_fd);
		else	  fioClose(in_fd);
	}
	if(in_fd>0){
		if(hddout) fileXioClose(out_fd);
		else	  fioClose(out_fd);
	}
	return ret;
}

////////////////////////////////////////////////////////////////////////
// �y�[�X�g
int paste(const char *path)
{
	char tmp[MAX_PATH];
	int i, ret=-1;
	
	if(!strcmp(path,clipPath)) return -1;
	
	for(i=0; i<nclipFiles; i++){
		strcpy(tmp, clipFiles[i].name);
		if(clipFiles[i].attr & FIO_S_IFDIR) strcat(tmp,"/");
		strcat(tmp, " pasting");
		drawMsg(tmp);
		ret=copy(path, clipPath, clipFiles[i], 0);
		if(ret < 0) break;
		if(cut){
			ret=delete(clipPath, &clipFiles[i]);
			if(ret<0) break;
		}
	}
	
	if(mountedParty[0][0]!=0){
		fileXioUmount("pfs0:"); mountedParty[0][0]=0;
	}
	
	if(mountedParty[1][0]!=0){
		fileXioUmount("pfs1:"); mountedParty[1][0]=0;
	}
	
	return ret;
}

////////////////////////////////////////////////////////////////////////
// �X�N���[���L�[�{�[�h
/*
�� �g�p�s����
 : * " | < > \ / ?
�� ���C�A�E�g
A B C D E F G H I J K L M
N O P Q R S T U V W X Y Z
a b c d e f g h i j k l m
n o p q r s t u v w x y z
0 1 2 3 4 5 6 7 8 9      
( ) [ ] ! # $ % & @ ;    
= + - ' ^ . , _          
OK                  CANCEL
*/
int keyboard(char *out, int max)
{
	const int	KEY_W=276,
				KEY_H=84,
				KEY_X=130,
				KEY_Y=60,
				WFONTS=13,
				HFONTS=7;
	char *KEY="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789   ()[]!#$%&@;  =+-'^.,_     ";
	int KEY_LEN;
	int cur=0, sel=0, i, x, y, t=0;
	char tmp[256], *p;
	
	p=strrchr(out, '.');
	if(p==NULL)	cur=strlen(out);
	else		cur=(int)(p-out);
	KEY_LEN = strlen(KEY);
	while(1){
		waitPadReady(0, 0);
		if(readpad()){
			if(new_pad & PAD_UP){
				if(sel<=WFONTS*HFONTS){
					if(sel>=WFONTS) sel-=WFONTS;
				}else{
					sel-=4;
				}
			}else if(new_pad & PAD_DOWN){
				if(sel/WFONTS == HFONTS-1){
					if(sel%WFONTS < 5)	sel=WFONTS*HFONTS;
					else				sel=WFONTS*HFONTS+1;
				}else if(sel/WFONTS <= HFONTS-2)
					sel+=WFONTS;
			}else if(new_pad & PAD_LEFT){
				if(sel>0) sel--;
			}else if(new_pad & PAD_RIGHT){
				if(sel<=WFONTS*HFONTS) sel++;
			}else if(new_pad & PAD_START){
				sel = WFONTS*HFONTS;
			}else if(new_pad & PAD_L1){
				if(cur>0) cur--;
				t=0;
			}else if(new_pad & PAD_R1){
				if(cur<strlen(out)) cur++;
				t=0;
			}else if(new_pad & PAD_CROSS){
				if(cur>0){
					strcpy(tmp, out);
					out[cur-1]=0;
					strcat(out, &tmp[cur]);
					cur--;
					t=0;
				}
			}else if(new_pad & PAD_CIRCLE){
				i=strlen(out);
				if(sel < WFONTS*HFONTS){
					if(i<max && i<33){
						strcpy(tmp, out);
						out[cur]=KEY[sel];
						out[cur+1]=0;
						strcat(out, &tmp[cur]);
						cur++;
						t=0;
					}
				}else if(sel == WFONTS*HFONTS && i>0){
					break;
				}else
					return -1;
			}
		}
		// �`��J�n
		itoSprite(setting->color[0],
			KEY_X-2,
			KEY_Y-1,
			KEY_X+KEY_W+3,
			KEY_Y+KEY_H+2, 0);
		drawFrame(
			KEY_X,
			KEY_Y,
			KEY_X+KEY_W,
			KEY_Y+KEY_H,setting->color[1]);
		itoLine(setting->color[1], KEY_X, KEY_Y+11, 0,
			setting->color[1], KEY_X+KEY_W, KEY_Y+11, 0);
		printXY(out,
			KEY_X+2+3,
			KEY_Y+2,
			setting->color[3], TRUE);
		t++;
		if(t<SCANRATE/2){
			printXY("|",
				KEY_X+cur*8+1,
				KEY_Y+2,
				setting->color[3], TRUE);
		}else{
			if(t==SCANRATE) t=0;
		}
		for(i=0; i<KEY_LEN; i++)
			drawChar(KEY[i],
				KEY_X+2+4 + (i%WFONTS+1)*20 - 12,
				KEY_Y+16 + (i/WFONTS)*8,
				setting->color[3]);
		printXY("OK                       CANCEL",
			KEY_X+2+4 + 20 - 12,
			KEY_Y+16 + HFONTS*8,
			setting->color[3], TRUE);
		if(sel<=WFONTS*HFONTS)
			x = KEY_X+2+4 + (sel%WFONTS+1)*20 - 20;
		else
			x = KEY_X+2+4 + 25*8;
		y = KEY_Y+16 + (sel/WFONTS)*8;
		drawChar(127, x, y, setting->color[3]);
		
		// �������
		x = SCREEN_MARGIN;
		y = SCREEN_HEIGHT-SCREEN_MARGIN-FONT_HEIGHT;
		itoSprite(setting->color[0],
			0,
			y/2,
			SCREEN_WIDTH,
			y/2+8, 0);
		printXY("��:OK �~:Back L1:Left R1:Right START:Enter",
			x, y/2, setting->color[2], TRUE);
		drawScr();
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////
// �t�@�C�����X�g�ݒ�
int setFileList(const char *path, const char *ext, FILEINFO *files, int cnfmode)
{
	char *p;
	int nfiles, i, j, ret;
	
	// �t�@�C�����X�g�ݒ�
	if(path[0]==0){
		strcpy(files[0].name, "mc0:");
		strcpy(files[1].name, "mc1:");
		strcpy(files[2].name, "hdd0:");
		strcpy(files[3].name, "cdfs:");
		strcpy(files[4].name, "mass:");
		files[0].attr = FIO_S_IFDIR;
		files[1].attr = FIO_S_IFDIR;
		files[2].attr = FIO_S_IFDIR;
		files[3].attr = FIO_S_IFDIR;
		files[4].attr = FIO_S_IFDIR;
		nfiles = 5;
		for(i=0; i<nfiles; i++)
			files[i].title[0]=0;
		if(cnfmode){
			strcpy(files[nfiles].name, "MISC");
			files[nfiles].attr = FIO_S_IFDIR;
			nfiles++;
		}
		vfreeSpace=FALSE;
	}else if(!strcmp(path, "MISC/")){
		strcpy(files[0].name, "..");
		strcpy(files[1].name, "FileBrowser");
		strcpy(files[2].name, "PS2Browser");
		strcpy(files[3].name, "PS2Disc");
		files[0].attr = FIO_S_IFDIR;
		files[1].attr = FIO_S_IFREG;
		files[2].attr = FIO_S_IFREG;
		files[3].attr = FIO_S_IFREG;
		nfiles = 4;
		for(i=0; i<nfiles; i++)
			files[i].title[0]=0;
	}else{
		strcpy(files[0].name, "..");
		files[0].attr = FIO_S_IFDIR;
		nfiles = getDir(path, &files[1]) + 1;
		if(strcmp(ext,"*")){
			for(i=j=1; i<nfiles; i++){
				if(files[i].attr & FIO_S_IFDIR)
					files[j++] = files[i];
				else{
					p = strrchr(files[i].name, '.');
					if(p!=NULL && !stricmp(ext,p+1))
						files[j++] = files[i];
				}
			}
			nfiles = j;
		}
		if(title){
			for(i=1; i<nfiles; i++){
				ret = getGameTitle(path, &files[i], files[i].title);
				if(ret<0) files[i].title[0]=0;
			}
		}
		if(!strcmp(path, "hdd0:/"))
			vfreeSpace=FALSE;
		else if(nfiles>1)
			sort(&files[1], 0, nfiles-2);
	}
	
	return nfiles;
}

////////////////////////////////////////////////////////////////////////
// �C�ӂ̃t�@�C���p�X��Ԃ�
void getFilePath(char *out, int cnfmode)
{
	char path[MAX_PATH], oldFolder[MAX_PATH],
		msg0[MAX_PATH], msg1[MAX_PATH],
		tmp[MAX_PATH], ext[8], *p;
	uint64 color;
	FILEINFO files[MAX_ENTRY];
	int nfiles=0, sel=0, top=0, rows=MAX_ROWS;
	int cd=TRUE, up=FALSE, pushed=TRUE;
	int nofnt=FALSE;
	int x, y, y0, y1;
	int i, ret, fd;
	size_t size;
	
	if(cnfmode) strcpy(ext, "elf");
	else		strcpy(ext, "*");
	strcpy(path, LastDir);
	mountedParty[0][0]=0;
	mountedParty[1][0]=0;
	clipPath[0] = 0;
	nclipFiles = 0;
	cut = 0;
	title=FALSE;
	while(1){
		waitPadReady(0, 0);
		if(readpad()){
			if(new_pad) pushed=TRUE;
			if(new_pad & PAD_UP)
				sel--;
			else if(new_pad & PAD_DOWN)
				sel++;
			else if(new_pad & PAD_LEFT)
				sel-=rows/2;
			else if(new_pad & PAD_RIGHT)
				sel+=rows/2;
			else if(new_pad & PAD_TRIANGLE)
				up=TRUE;
			else if(new_pad & PAD_CIRCLE){
				if(files[sel].attr & FIO_S_IFDIR){
					if(!strcmp(files[sel].name,".."))
						up=TRUE;
					else {
						strcat(path, files[sel].name);
						strcat(path, "/");
						cd=TRUE;
					}
				}else{
					sprintf(out, "%s%s", path, files[sel].name);
					if(checkELFheader(out)<0){
						pushed=FALSE;
						sprintf(msg0, "This file isn't ELF.");
					}else{
						strcpy(LastDir, path);
						break;
					}
				}
			}
			if(cnfmode){
				// �t�@�C���}�X�N�؂�ւ�
				if(new_pad & PAD_SQUARE) {
					if(!strcmp(ext,"*")) strcpy(ext, "elf");
					else				 strcpy(ext, "*");
					cd=TRUE;
				// ���C�����j���[�ɖ߂�
				} else if(new_pad & PAD_CROSS){
					if(mountedParty[0][0]!=0) fileXioUmount("pfs0:");
					return;
				}
			}else{
				// ���j���[
				if(new_pad & PAD_R1) {
					ret = menu(path, files[sel].name);
					// �N���b�v�{�[�h�ɃR�s�[
					if(ret==COPY || ret==CUT){
						strcpy(clipPath, path);
						if(nmarks>0){
							for(i=nclipFiles=0; i<nfiles; i++)
								if(marks[i])
									clipFiles[nclipFiles++]=files[i];
						}else{
							clipFiles[0]=files[sel];
							nclipFiles = 1;
						}
						sprintf(msg0, "Copied to the Clipboard");
						pushed=FALSE;
						if(ret==CUT)	cut=TRUE;
						else			cut=FALSE;
					// �f���[�g
					} else if(ret==DELETE){
						if(nmarks==0){
							if(title && files[sel].title[0])
								sprintf(tmp,"%s",files[sel].title);
							else{
								sprintf(tmp,"%s",files[sel].name);
								if(files[sel].attr & FIO_S_IFDIR)
									strcat(tmp,"/");
							}
							strcat(tmp, "\nDelete?");
							ret = ynDialog(tmp);
						}else
							ret = ynDialog("Mark Files Delete?");
						
						if(ret>0){
							if(nmarks==0){
								strcpy(tmp, files[sel].name);
								if(files[sel].attr & FIO_S_IFDIR) strcat(tmp,"/");
								strcat(tmp, " deleting");
								drawMsg(tmp);
								ret=delete(path, &files[sel]);
							}else{
								for(i=0; i<nfiles; i++){
									if(marks[i]){
										strcpy(tmp, files[i].name);
										if(files[i].attr & FIO_S_IFDIR) strcat(tmp,"/");
										strcat(tmp, " deleting");
										drawMsg(tmp);
										ret=delete(path, &files[i]);
										if(ret<0) break;
									}
								}
							}
							if(ret>=0) cd=TRUE;
							else{
								strcpy(msg0, "Delete Failed");
								pushed = FALSE;
							}
						}
					// ���l�[��
					} else if(ret==RENAME){
						strcpy(tmp, files[sel].name);
						if(keyboard(tmp, 36)>=0){
							if(Rename(path, &files[sel], tmp)<0){
								pushed=FALSE;
								strcpy(msg0, "Rename Failed");
							}else
								cd=TRUE;
						}
					// �N���b�v�{�[�h����y�[�X�g
					} else if(ret==PASTE){
						drawMsg("Pasting...");
						ret=paste(path);
						if(ret < 0){
							strcpy(msg0, "Paste Failed");
							pushed = FALSE;
						}else
							if(cut) nclipFiles=0;
						cd=TRUE;
					// �V�K�t�H���_�쐬
					} else if(ret==NEWDIR){
						tmp[0]=0;
						if(keyboard(tmp, 36)>=0){
							ret = newdir(path, tmp);
							if(ret == -17){
								strcpy(msg0, "directory already exists");
								pushed=FALSE;
							}else if(ret < 0){
								strcpy(msg0, "NewDir Failed");
								pushed=FALSE;
							}else{
								strcat(path, tmp);
								strcat(path, "/");
								cd=TRUE;
							}
						}
					// �T�C�Y�\��
					} else if(ret==GETSIZE){
						drawMsg("Checking Size...");
						if(nmarks==0){
							size=getFileSize(path, &files[sel]);
						}else{
							for(i=size=0; i<nfiles; i++){
								if(marks[i])
									size+=getFileSize(path, &files[i]);
								if(size<0) size=-1;
							}
						}
						if(size<0){
							strcpy(msg0, "Paste Failed");
						}else{
							if(size >= 1024*1024)
								sprintf(msg0, "SIZE = %.1fMB", (double)size/1024/1024);
							else if(size >= 1024)
								sprintf(msg0, "SIZE = %.1fKB", (double)size/1024);
							else
								sprintf(msg0, "SIZE = %dB", size);
						}
						pushed = FALSE;
					}
				// �}�[�N
				} else if(new_pad & PAD_CROSS) {
					if(sel!=0 && path[0]!=0 && strcmp(path,"hdd0:/")){
						if(marks[sel]){
							marks[sel]=FALSE;
							nmarks--;
						}else{
							marks[sel]=TRUE;
							nmarks++;
						}
					}
					sel++;
				// �}�[�N���]
				} else if(new_pad & PAD_SQUARE) {
					if(path[0]!=0 && strcmp(path,"hdd0:/")){
						for(i=1; i<nfiles; i++){
							if(marks[i]){
								marks[i]=FALSE;
								nmarks--;
							}else{
								marks[i]=TRUE;
								nmarks++;
							}
						}
					}
				// �^�C�g���\���؂�ւ�
				} else if(new_pad & PAD_L1) {
					if(elisaFnt==NULL && nofnt==FALSE){
						sprintf(tmp, "%s%s", LaunchElfDir, "ELISA100.FNT");
						if(!strncmp(tmp, "cdrom", 5)) strcat(tmp, ";1");
						fd = fioOpen(tmp, O_RDONLY);
						if(fd>0){
							ret = fioLseek(fd,0,SEEK_END);
							if(ret==55016){
								elisaFnt = (char*)malloc(ret);
								fioLseek(fd,0,SEEK_SET);
								fioRead(fd, elisaFnt, ret);
								fioClose(fd);
							}
						}else
							nofnt = TRUE;
					}
					title = !title;
					if(elisaFnt!=NULL){
						if(title) rows=19;
						else	  rows=MAX_ROWS;
					}
					cd=TRUE;
				// ���C�����j���[�ɖ߂�
				} else if(new_pad & PAD_SELECT){
					if(mountedParty[0][0]!=0) fileXioUmount("pfs0:");
					if(mountedParty[1][0]!=0) fileXioUmount("pfs1:");
					return;
				}
			}
		}
		// ��ʃt�H���_�ړ�
		if(up){
			if((p=strrchr(path, '/'))!=NULL)
				*p = 0;
			if((p=strrchr(path, '/'))!=NULL){
				p++;
				strcpy(oldFolder, p);
				*p = 0;
			}else{
				strcpy(oldFolder, path);
				path[0] = 0;
			}
			cd=TRUE;
		}
		// �t�H���_�ړ�
		if(cd){
			nfiles = setFileList(path, ext, files, cnfmode);
			// �󂫗e�ʎ擾
			if(!cnfmode){
				if(!strncmp(path, "mc", 2)){
					mcGetInfo(path[2]-'0', 0, NULL, &mcfreeSpace, NULL);
				}else if(!strncmp(path,"hdd",3)&&strcmp(path,"hdd0:/")){
					freeSpace = 
					fileXioDevctl("pfs0:",PFSCTL_GET_ZONE_FREE,NULL,0,NULL,0)*fileXioDevctl("pfs0:",PFSCTL_GET_ZONE_SIZE,NULL,0,NULL,0);
					vfreeSpace=TRUE;
				}
			}
			// �ϐ�������
			sel=0;
			top=0;
			if(up){
				for(i=0; i<nfiles; i++) {
					if(!strcmp(oldFolder, files[i].name)) {
						sel=i;
						top=sel-3;
						break;
					}
				}
			}
			nmarks = 0;
			memset(marks, 0, MAX_ENTRY);
			cd=FALSE;
			up=FALSE;
		}
		if(strncmp(path,"cdfs",4) && setting->discControl)
			CDVD_Stop();
		// �t�@�C�����X�g�\���p�ϐ��̐��K��
		if(top > nfiles-rows)	top=nfiles-rows;
		if(top < 0)				top=0;
		if(sel >= nfiles)		sel=nfiles-1;
		if(sel < 0)				sel=0;
		if(sel >= top+rows)		top=sel-rows+1;
		if(sel < top)			top=sel;
		
		// ��ʕ`��J�n
		clrScr(setting->color[0]);
		// �t�@�C�����X�g
		x = SCREEN_MARGIN + LINE_THICKNESS + FONT_WIDTH;
		y = SCREEN_MARGIN + FONT_HEIGHT*2 + LINE_THICKNESS + 12;
		if(title && elisaFnt!=NULL) y-=2;
		for(i=0; i<rows; i++)
		{
			if(top+i >= nfiles) break;
			if(top+i == sel) color = setting->color[2];
			else			 color = setting->color[3];
			if(files[top+i].attr & FIO_S_IFDIR){
				if(!strcmp(files[top+i].name,".."))
					strcpy(tmp,"..");
				else if(title && files[top+i].title[0]!=0)
					strcpy(tmp,files[top+i].title);
				else
					sprintf(tmp, "%s/", files[top+i].name);
			}else
				strcpy(tmp,files[top+i].name);
			printXY(tmp, x+4, y/2, color, TRUE);
			if(marks[top+i]) drawChar('*', x-6, y/2, setting->color[3]);
			y += FONT_HEIGHT;
			if(title && elisaFnt!=NULL) y+=2;
		}
		// �X�N���[���o�[
		if(nfiles > rows)
		{
			itoSprite(setting->color[1],
				SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-14,
				(SCREEN_MARGIN+FONT_HEIGHT*2+4)/2,
				SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-12,
				(SCREEN_HEIGHT-SCREEN_MARGIN-FONT_HEIGHT)/2,
				0);
			y0=(SCREEN_HEIGHT-SCREEN_MARGIN*2-FONT_HEIGHT*3-LINE_THICKNESS*2-4)
				* ((double)top/nfiles);
			y1=(SCREEN_HEIGHT-SCREEN_MARGIN*2-FONT_HEIGHT*3-LINE_THICKNESS*2-4)
				* ((double)(top+rows)/nfiles);
			itoSprite(setting->color[1],
				SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-11,
				(y0+SCREEN_MARGIN+FONT_HEIGHT*2+LINE_THICKNESS+4)/2,
				SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-1,
				(y1+SCREEN_MARGIN+FONT_HEIGHT*2+LINE_THICKNESS+4)/2,
				0);
		}
		// ���b�Z�[�W
		if(pushed) sprintf(msg0, "Path: %s", path);
		// �������
		if(cnfmode){
			if(!strcmp(ext, "*"))
				sprintf(msg1, "��:OK �~:Cancel ��:Up ��:*->ELF");
			else
				sprintf(msg1, "��:OK �~:Cancel ��:Up ��:ELF->*");
		}else{
			if(title)
				sprintf(msg1, "��:OK ��:Up �~:Mark ��:RevMark L1:TitleOFF R1:Menu");
			else
				sprintf(msg1, "��:OK ��:Up �~:Mark ��:RevMark L1:TitleON  R1:Menu");
		}
		setScrTmp(msg0, msg1);
		// �t���[�X�y�[�X�\��
		if(!strncmp(path, "mc", 2) && !vfreeSpace && !cnfmode){
			if(mcSync(1,NULL,NULL)!=0){
				freeSpace = mcfreeSpace*1024;
				vfreeSpace=TRUE;
			}
		}
		if(vfreeSpace){
			if(freeSpace >= 1024*1024)
				sprintf(tmp, "[%.1fMB free]", (double)freeSpace/1024/1024);
			else if(freeSpace >= 1024)
				sprintf(tmp, "[%.1fKB free]", (double)freeSpace/1024);
			else
				sprintf(tmp, "[%dB free]", freeSpace);
			ret=strlen(tmp);
			itoSprite(setting->color[0],
				SCREEN_WIDTH-SCREEN_MARGIN-(ret+1)*8,
				(SCREEN_MARGIN+FONT_HEIGHT+4)/2,
				SCREEN_WIDTH,
				(SCREEN_MARGIN+FONT_HEIGHT+20)/2, 0);
			printXY(tmp,
				SCREEN_WIDTH-SCREEN_MARGIN-ret*8,
				(SCREEN_MARGIN+FONT_HEIGHT+4)/2,
				setting->color[2], TRUE);
		}
		
		drawScr();
	}
	
	if(mountedParty[0][0]!=0) fileXioUmount("pfs0:");
	if(mountedParty[1][0]!=0) fileXioUmount("pfs1:");
	return;
}