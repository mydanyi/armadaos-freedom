/* SPDX-License-Identifier: GPL-3.0-only */
#include <storage_ufs.h>
#include <string.h>

/* Original freestanding implementation. Wire/register definitions checked
 * against Linux 330517c63aa9b48227b6ab45f01003effac77ae4:
 * include/ufs/{ufs,ufshci}.h and include/uapi/scsi/scsi_bsg_ufs.h.
 * Legacy list, tag0, one PRDT segment, no crypto, no interrupts, no resets.
 * Qualcomm clock/PHY/SMMU setup belongs to the board adapter, not this core. */
enum { CAP=0, AHIT=0x18, IS=0x20, IE=0x24, HCS=0x30, HCE=0x34,
       BASE_LO=0x50, BASE_HI=0x54, DOORBELL=0x58, RUN=0x60,
       TASK_DB=0x78, UIC_CMD=0x90, UIC_ARG1=0x94, UIC_ARG2=0x98,
       UIC_ARG3=0x9c, MEM_CFG=0x300, UCD=1024, RSP=1536,
       PRDT=2048, DATA=4096, ERRORS=0x71884 };
/* UIC DME commands, the completion bits they raise, and the MPHY attribute
 * selector that returns the TX FSM state (ufshci.h UIC_GET/HIBERN8 opcodes,
 * UCCS/UHXS/UHES, HCS.UICRDY and HCS.UPMCRS, MPHY_TX_FSM_STATE on lane 0).
 * TX FSM values follow Linux drivers/ufs/host/ufshcd-dwc.h enum tx_fsm:
 * 0 DISABLED, 1 HIBERN8, 2 SLEEP, 3 STALL, 4 LSBURST, 5 HSBURST. SLEEP,
 * STALL, LSBURST and HSBURST are the known normal awake states; 0, 6 and
 * 0xff are unknown and are refused. */
enum { UIC_GET=0x01, UIC_HIBERN8_ENTER=0x17, UIC_HIBERN8_EXIT=0x18,
       IS_UIC=0x400, IS_EXIT=0x20, IS_ENTER=0x40,
       HCS_READY=15, HCS_UIC_READY=0x8, HCS_ERROR=0x30, HCS_PWR_LOCAL=1,
       MPHY_TX_FSM=0x00410000, TX_HIBERN8=1, TX_SLEEP=2, TX_STALL=3,
       TX_LSBURST=4, TX_HSBURST=5 };
enum { UIC_POLL_ITERS=200000, UIC_POLL_US=10, UIC_POLL_MS=1000 };
/* bCurrentPowerMode attribute values are NOT the SCSI POWER CONDITION
 * enum used by Linux ufshcd_set_dev_pwr_mode(). Keep raw attribute evidence
 * and translate only at the command boundary. Linux upstream discussion:
 * https://lkml.indiana.edu/hypermail/linux/kernel/1809.3/06550.html */
enum { ATTR_ACTIVE=0x11, ATTR_SLEEP=0x22, ATTR_POWERDOWN=0x33,
       SSU_ACTIVE=1, SSU_SLEEP=2, SSU_POWERDOWN=3 };
static uint8_t power_condition(uint32_t attribute)
{
    switch(attribute){
    case ATTR_ACTIVE:return SSU_ACTIVE;
    case ATTR_SLEEP:return SSU_SLEEP;
    case ATTR_POWERDOWN:return SSU_POWERDOWN;
    default:return 0;
    }
}
static void le16(uint8_t *p,uint16_t v){p[0]=v;p[1]=v>>8;}
static void le32(uint8_t *p,uint32_t v){le16(p,v);le16(p+2,v>>16);}
static void le64(uint8_t *p,uint64_t v){le32(p,(uint32_t)v);le32(p+4,v>>32);}
static void be32(uint8_t *p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
static void be64(uint8_t *p,uint64_t v){be32(p,v>>32);be32(p+4,(uint32_t)v);}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static uint64_t get64(const uint8_t *p){return (uint64_t)get32(p)<<32|get32(p+4);}
static uint16_t get16(const uint8_t *p){return (uint16_t)p[0]<<8|p[1];}
static int rd(struct kshim_ufs_host *h,uint32_t r,uint32_t *v)
{ return h->ops.read32(h->ops.ctx,r,v)?KSHIM_STORAGE_IO:0; }
static int wr(struct kshim_ufs_host *h,uint32_t r,uint32_t v)
{ return h->ops.write32(h->ops.ctx,r,v)?KSHIM_STORAGE_IO:0; }
static int wait_clear_for(struct kshim_ufs_host *h,uint32_t r,uint32_t mask,
                          uint32_t milliseconds,unsigned iterations)
{
    uint64_t start=h->ops.now_ms(h->ops.ctx);
    /* The iteration bound also terminates if a supplied timer stops. */
    for(unsigned i=0;i<iterations;++i){
        uint32_t v;int rc=rd(h,r,&v);if(rc)return rc;
        if(!(v&mask))return 0;
        if(h->ops.now_ms(h->ops.ctx)-start>=milliseconds)return KSHIM_UFS_TIMEOUT;
        h->ops.delay_us(h->ops.ctx,10);
    }
    return KSHIM_UFS_TIMEOUT;
}
static int wait_clear(struct kshim_ufs_host *h,uint32_t r,uint32_t mask)
{ return wait_clear_for(h,r,mask,1000,200000); }
/* One polled UIC DME command for the opt-in manage_link path. The controller
 * only accepts these while our list is stopped and its queues are drained, so
 * every call re-checks that state instead of trusting the caller. Only the
 * completion bits this command is defined to raise are cleared beforehand and
 * acknowledged afterwards. uncertain is raised before the trigger write
 * because a failed callback may already have taken effect, and is cleared only
 * by a command whose result is known to be successful. UPMCRS is a report on
 * the previous power command, not an admission test for the next one, so the
 * power state is only judged from a fresh completion. */
static int uic_cmd(struct kshim_ufs_host *h,uint32_t command,uint32_t arg1,
                   uint32_t required,bool power)
{
    uint32_t hce,hcs,run,db,tdb,is,v;int rc;
    if((rc=rd(h,HCE,&hce))||(rc=rd(h,HCS,&hcs))||(rc=rd(h,RUN,&run))||
       (rc=rd(h,DOORBELL,&db))||(rc=rd(h,TASK_DB,&tdb))||
       (rc=rd(h,IS,&is)))return rc;
    /* A stopped list withdraws the transfer/task list ready bits, so only the
     * UIC-ready bit3 gates the command here; the full ready mask is checked
     * once, before the claim stops the list. */
    if(!(hce&1)||!(hcs&HCS_UIC_READY)||(hcs&HCS_ERROR)||(run&1)||
       db||tdb||(is&ERRORS))return KSHIM_UFS_BUSY;
    h->link.power_status=(hcs>>8)&7;
    h->link.last_command=command;
    if((rc=wr(h,IS,required)))return rc;
    if((rc=wr(h,UIC_ARG1,arg1))||(rc=wr(h,UIC_ARG2,0))||
       (rc=wr(h,UIC_ARG3,0)))return rc;
    /* Both obligations are recorded at the trigger: the exit may already have
     * happened, and only an exit that was actually issued must be undone.
     * A command still being formed owes nothing and leaves the link alone. */
    h->link.uncertain=1;
    if(command==UIC_HIBERN8_EXIT)h->link.restore_hibern8=1;
    if((rc=wr(h,UIC_CMD,command)))return rc;
    uint64_t start=h->ops.now_ms(h->ops.ctx);int done=0;
    /* Both bounds stand: a frozen timer cannot remove the iteration ceiling,
     * and one status read per iteration keeps that ceiling meaningful. */
    for(unsigned i=0;i<UIC_POLL_ITERS;++i){
        if((rc=rd(h,IS,&is)))return rc;
        h->link.last_is=is;
        if(is&ERRORS)return KSHIM_STORAGE_IO;
        if((is&required)==required){done=1;break;}
        if(h->ops.now_ms(h->ops.ctx)-start>=UIC_POLL_MS)return KSHIM_UFS_TIMEOUT;
        h->ops.delay_us(h->ops.ctx,UIC_POLL_US);
    }
    if(!done)return KSHIM_UFS_TIMEOUT;
    /* Nothing is acknowledged until the whole required mask was observed. */
    if((rc=wr(h,IS,required)))return rc;
    if((rc=rd(h,UIC_ARG2,&v)))return rc;
    h->link.command_result=v&0xffu;
    /* ARG2 carries the result/error code. A rejected read-only query completed
     * and changed nothing, so the link stays usable; a rejected power command
     * leaves the link unknown. */
    if(v&0xffu){if(!power)h->link.uncertain=0;return KSHIM_UFS_RESPONSE;}
    if(power){
        if((rc=rd(h,HCS,&hcs)))return rc;
        h->link.power_status=(hcs>>8)&7;
        if(h->link.power_status!=HCS_PWR_LOCAL)return KSHIM_UFS_RESPONSE;
    } else {
        /* ARG3, never ARG2, returns the TX FSM state. */
        if((rc=rd(h,UIC_ARG3,&v)))return rc;
        h->link.current_tx=v;
    }
    h->link.uncertain=0;
    return 0;
}
/* Known normal awake states: SLEEP, STALL, LSBURST and HSBURST. */
static bool tx_awake(uint32_t tx)
{ return tx>=TX_SLEEP&&tx<=TX_HSBURST; }
/* Phases 1-3 of the opt-in claim: read the state firmware left behind, exit
 * HIBERN8 if that is what it is, and confirm the awake state. Anything other
 * than a known awake state is refused before any DMA is installed. */
static int link_prepare(struct kshim_ufs_host *h)
{
    int rc;
    h->link.initial_tx=UINT32_MAX;h->link.current_tx=UINT32_MAX;
    h->link.command_result=UINT32_MAX;h->link.phase=1;
    if((rc=uic_cmd(h,UIC_GET,MPHY_TX_FSM,IS_UIC,false)))return rc;
    h->link.initial_tx=h->link.current_tx;
    if(tx_awake(h->link.initial_tx))return 0;
    if(h->link.initial_tx!=TX_HIBERN8)return KSHIM_UFS_UNSUPPORTED;
    /* uic_cmd records the restore obligation when it writes the trigger, so a
     * failure that leaves the link untouched still owes no re-entry. */
    h->link.phase=2;
    if((rc=uic_cmd(h,UIC_HIBERN8_EXIT,0,IS_UIC|IS_EXIT,true)))return rc;
    h->link.phase=3;
    if((rc=uic_cmd(h,UIC_GET,MPHY_TX_FSM,IS_UIC,false)))return rc;
    if(!tx_awake(h->link.current_tx))return KSHIM_UFS_RESPONSE;
    return 0;
}
int kshim_ufs_claim(struct kshim_ufs_host *h,const struct kshim_ufs_ops *o,
                    void *arena,size_t bytes,uint64_t dma)
{
    if(!h||!o||!arena||bytes<KSHIM_UFS_DMA_BYTES||
       ((uintptr_t)arena&1023)||!dma||(dma&1023)||dma>UINT64_MAX-8191||
       !o->read32||!o->write32||!o->sync_device||!o->sync_cpu||
       !o->now_ms||!o->delay_us)return KSHIM_STORAGE_ARG;
    if(h->owned)return KSHIM_UFS_BUSY;
    memset(h,0,sizeof(*h));h->ops=*o;h->arena=arena;h->dma_address=dma;
    uint32_t cap,hce,hcs,db,tdb,mode=0,status;
    int rc;
#define READ(r,v) do { rc=rd(h,r,&v);if(rc)return rc; } while(0)
    READ(CAP,cap);READ(HCE,hce);READ(HCS,hcs);
    READ(DOORBELL,db);READ(TASK_DB,tdb);READ(IS,status);
    if(cap&(1u<<30)){READ(MEM_CFG,mode);}
    if(!(hce&1)||(hcs&15)!=15||(hcs&0x30)||db||tdb||(status&ERRORS))
        return KSHIM_UFS_BUSY;
    if((cap&(1u<<29))||(mode&1)||(!(cap&(1u<<24))&&dma+8191>UINT32_MAX))
        return KSHIM_UFS_UNSUPPORTED;
    READ(BASE_LO,h->saved_base[0]);READ(BASE_HI,h->saved_base[1]);
    READ(RUN,h->saved_run);READ(IE,h->saved_ie);READ(AHIT,h->saved_ahit);
#undef READ
    /* Any failure after our first write requires successful release before
     * freeing DMA memory or handing off. State is never erased on an error. */
    h->owned=1;h->poisoned=1;
    if((rc=wr(h,IE,0))||(rc=wr(h,AHIT,0))||(rc=wr(h,RUN,0))||
       (rc=wait_clear(h,RUN,1)))return rc;
    if((rc=rd(h,DOORBELL,&db)))return rc;
    if(db)return KSHIM_UFS_BUSY;
    /* Link work runs with our list stopped and the queues drained, and before
     * any arena or base address is handed to the controller. */
    if(h->ops.manage_link){
        if((rc=rd(h,TASK_DB,&tdb)))return rc;
        if(tdb)return KSHIM_UFS_BUSY;
        if((rc=link_prepare(h)))return rc;
    }
    memset(arena,0,KSHIM_UFS_DMA_BYTES);
    h->ops.sync_device(h->ops.ctx,arena,KSHIM_UFS_DMA_BYTES);
    if((rc=wr(h,BASE_LO,(uint32_t)dma))||(rc=wr(h,BASE_HI,dma>>32))||
       (rc=wr(h,IS,1))||(rc=wr(h,RUN,1)))return rc;
    h->poisoned=0;return 0;
}
static int transfer(struct kshim_ufs_host *h,uint8_t kind,uint8_t lun,
                     const uint8_t *command,uint32_t bytes,int direction,
                     void *data,uint8_t descriptor,uint8_t index,uint16_t *length,
                     uint8_t query_opcode)
{
    if(!h||!h->owned)return KSHIM_STORAGE_ARG;
    if(h->poisoned)return KSHIM_UFS_QUARANTINED;
    if(bytes>4096||(bytes&&(!data||(bytes&3))))return KSHIM_STORAGE_ARG;
    uint32_t db,status;int rc;
    if((rc=rd(h,DOORBELL,&db)))return rc;
    if(db){h->poisoned=1;return KSHIM_UFS_BUSY;}
    memset(h->arena,0,KSHIM_UFS_DMA_BYTES);
    uint8_t *t=h->arena,*q=t+UCD,*rsp=t+RSP;
    le32(t,(1u<<28)|(1u<<24)|((uint32_t)direction<<25));
    t[8]=15;le64(t+16,h->dma_address+UCD);
    le16(t+24,128);le16(t+26,128);le16(t+28,bytes?1:0);le16(t+30,256);
    q[0]=kind;q[2]=lun; /* fixed task tag0 */
    if(kind==1){
        q[1]=direction==2?0x40:direction==1?0x20:0;
        be32(q+12,bytes);memcpy(q+16,command,16);
    } else if(kind==0x16){
        q[5]=query_opcode==4?0x81:1;q[12]=query_opcode;q[13]=descriptor;q[14]=index;
        if(query_opcode==4)be32(q+20,*(uint32_t *)data);
        if(query_opcode==1){
            q[18]=(uint8_t)(*length>>8);q[19]=(uint8_t)*length;
        }
    }
    if(bytes){
        le64(t+PRDT,h->dma_address+DATA);le32(t+PRDT+12,bytes-1);
        if(direction==1)memcpy(t+DATA,data,bytes);
    }
    if((rc=wr(h,IS,1)))return rc;
    h->ops.sync_device(h->ops.ctx,t,KSHIM_UFS_DMA_BYTES);
    /* Ringing the doorbell may have taken effect even if its callback fails. */
    bool ssu=kind==1&&lun==0xd0&&command[0]==0x1b;
    h->poisoned=1;
    if(kind==1){
        h->last_scsi_opcode=command[0];h->last_scsi_lun=lun;
        h->last_scsi_status=0;h->last_sense_key=0;
        h->last_sense_asc=0;h->last_sense_ascq=0;h->last_sense_valid=0;
    }
    /* Record the obligation at the actual trigger: the callback may report an
     * error after its store reached the controller. Never guess its outcome. */
    if(ssu){
        h->power.uncertain=1;
        h->power.restore_mode=power_condition(h->power.initial_mode);
    }
    if((rc=wr(h,DOORBELL,1)))return rc;
    if(ssu)rc=wait_clear_for(h,DOORBELL,1,10000,1200000);
    else rc=wait_clear(h,DOORBELL,1);
    if(rc)return rc;
    h->ops.sync_cpu(h->ops.ctx,t,KSHIM_UFS_DMA_BYTES);
    if((rc=rd(h,IS,&status)))return rc;
    if(status&ERRORS)return KSHIM_STORAGE_IO;
    if((rc=wr(h,IS,1)))return rc;
    h->poisoned=0;
    h->last_scsi_status=0;h->last_sense_key=0;
    h->last_sense_asc=0;h->last_sense_ascq=0;h->last_sense_valid=0;
    if((t[8]&15)!=0)return KSHIM_STORAGE_IO;
    /* response=1 is only meaningful as a failed SCSI command, and only when it
     * carries a nonzero status: NOP, descriptor queries, status0 and reserved
     * response codes stay rejected before any status is recorded. */
    if(rsp[0]!=(uint8_t)(kind+0x20)||rsp[3]!=0||rsp[2]!=lun||rsp[8]!=0||
       (rsp[6]&&(kind!=1||rsp[6]!=1||!rsp[7])))return KSHIM_UFS_RESPONSE;
    if(kind==1){
        h->last_scsi_status=rsp[7];
        if(rsp[7]){
            /* Only CHECK CONDITION carries autosense. The declared sense
             * length must be nonzero and fit the 18-byte sense area, and the
             * data segment must cover its length plus the two-byte length
             * field (max 480 inside the 512-byte response). The key is read
             * only from the byte the declared format puts it in: sense[2] for
             * fixed 0x70/0x71, sense[1] for descriptor 0x72/0x73. Malformed or
             * unknown layouts leave the key unknown so no retry is inferred. */
            uint16_t n=get16(rsp+32),segment=get16(rsp+10);
            if(rsp[7]==2&&n&&n<=18&&segment>=n+2&&segment<=480){
                uint8_t format=rsp[34]&0x7f;
                if((format==0x70||format==0x71)&&n>=3){
                    h->last_sense_key=rsp[36]&15;h->last_sense_valid=1;
                    if(n>=14&&rsp[41]>=6){
                        h->last_sense_asc=rsp[46];h->last_sense_ascq=rsp[47];
                        h->last_sense_valid|=2;
                    }
                } else if((format==0x72||format==0x73)&&n>=2){
                    h->last_sense_key=rsp[35]&15;h->last_sense_valid=1;
                    if(n>=4){
                        h->last_sense_asc=rsp[36];h->last_sense_ascq=rsp[37];
                        h->last_sense_valid|=2;
                    }
                }
            }
            /* A valid CHECK CONDITION is a completed command, even when its
             * sense is unusable. Release will query the actual power state. */
            if(ssu)h->power.uncertain=0;
            return KSHIM_UFS_SCSI;
        }
        if((rsp[1]&0x60)||get32(rsp+12))return KSHIM_UFS_RESPONSE;
        if(ssu)h->power.uncertain=0;
        if(direction==2&&bytes)memcpy(data,t+DATA,bytes);
    } else if(kind==0x16){
        uint16_t n=get16(rsp+18),segment=get16(rsp+10);
        if(rsp[7]||rsp[12]!=query_opcode||rsp[13]!=descriptor||
           rsp[14]!=index||rsp[15]!=0)return KSHIM_UFS_RESPONSE;
        if(query_opcode==3 || query_opcode==4){
            if(n||segment)return KSHIM_UFS_RESPONSE;
            *(uint32_t *)data=get32(rsp+20);
        } else {
            if(n<2||n>*length||n!=segment||n>255||
               rsp[32]!=n||rsp[33]!=descriptor)return KSHIM_UFS_RESPONSE;
            memcpy(data,rsp+32,n);*length=n;
        }
    } else if(rsp[7]||get16(rsp+10))return KSHIM_UFS_RESPONSE;
    return 0;
}
int kshim_ufs_nop(struct kshim_ufs_host *h)
{ return transfer(h,0,0,NULL,0,0,NULL,0,0,NULL,0); }
int kshim_ufs_get_boot_lun(struct kshim_ufs_host *h,uint32_t *value)
{
    if(!value)return KSHIM_STORAGE_ARG;
    int rc=transfer(h,0x16,0,NULL,0,0,value,0,0,NULL,3);
    return rc?rc:(*value==1||*value==2)?0:KSHIM_STORAGE_FORMAT;
}
int kshim_ufs_set_boot_lun(struct kshim_ufs_host *h,uint32_t value)
{
    if(value!=1 && value!=2)return KSHIM_STORAGE_ARG;
    uint32_t response=value,actual=0;
    int rc=transfer(h,0x16,0,NULL,0,0,&response,0,0,NULL,4);
    if(!rc)rc=kshim_ufs_get_boot_lun(h,&actual);
    return rc?rc:actual==value?0:KSHIM_STORAGE_IO;
}
int kshim_ufs_read_descriptor(struct kshim_ufs_host *h,uint8_t id,uint8_t index,
                              void *out,uint16_t *length)
{
    if(!out||!length||*length<2||*length>255)return KSHIM_STORAGE_ARG;
    return transfer(h,0x16,0,NULL,0,0,out,id,index,length,1);
}
static int scsi(struct kshim_ufs_host *h,uint8_t lun,const uint8_t *cdb,
                 void *data,uint32_t bytes,int direction)
{ return transfer(h,1,lun,cdb,bytes,direction,data,0,0,NULL,0); }
/* Linux ufshcd_query_attr() / ufshcd_set_dev_pwr_mode(): link resume and
 * device resume are separate operations. Query bCurrentPowerMode (IDN2), and
 * send START STOP UNIT only to the UFS device WLUN, never an ordinary LUN.
 * No query write, fDeviceInit, reset or disk write is involved. */
static int query_power(struct kshim_ufs_host *h)
{
    uint32_t mode;h->power.current_mode=UINT32_MAX;
    int rc=transfer(h,0x16,0,NULL,0,0,&mode,2,0,NULL,3);
    if(!rc)h->power.current_mode=mode;
    return rc;
}
static int set_power(struct kshim_ufs_host *h,uint8_t mode)
{
    uint8_t cdb[16]={0};cdb[0]=0x1b;cdb[4]=mode<<4;
    for(unsigned i=0;;++i){
        int rc=scsi(h,0xd0,cdb,NULL,0,0);
        if(!rc)return 0;
        if(i==2||rc!=KSHIM_UFS_SCSI||h->last_scsi_status!=2||
           !(h->last_sense_valid&1)||h->last_sense_key!=6)return rc;
    }
}
int kshim_ufs_prepare_device(struct kshim_ufs_host *h)
{
    if(!h||!h->owned)return KSHIM_STORAGE_ARG;
    if(h->poisoned||h->link.uncertain||h->power.uncertain)
        return KSHIM_UFS_QUARANTINED;
    if(h->power.phase)return KSHIM_UFS_BUSY;
    h->power.initial_mode=UINT32_MAX;h->power.phase=1;
    int rc=query_power(h);if(rc)return rc;
    h->power.initial_mode=h->power.current_mode;
    if(h->power.initial_mode==ATTR_ACTIVE)return 0;
    if(h->power.initial_mode!=ATTR_SLEEP&&h->power.initial_mode!=ATTR_POWERDOWN)
        return KSHIM_UFS_UNSUPPORTED;
    h->power.phase=2;
    if((rc=set_power(h,SSU_ACTIVE)))return rc;
    h->power.phase=3;
    if((rc=query_power(h)))return rc;
    return h->power.current_mode==ATTR_ACTIVE?0:KSHIM_UFS_RESPONSE;
}
static int restore_power(struct kshim_ufs_host *h)
{
    if(!h->power.restore_mode)return 0;
    if(h->poisoned)return KSHIM_UFS_QUARANTINED;
    h->power.phase=4;int rc=query_power(h);if(rc)return rc;
    if(h->power.current_mode!=h->power.initial_mode){
        if(h->power.current_mode!=ATTR_ACTIVE)return KSHIM_UFS_RESPONSE;
        h->power.phase=5;
        if((rc=set_power(h,h->power.restore_mode)))return rc;
        h->power.phase=6;
        if((rc=query_power(h)))return rc;
        if(h->power.current_mode!=h->power.initial_mode)return KSHIM_UFS_RESPONSE;
    }
    h->power.restore_mode=0;h->power.phase=7;
    return 0;
}
static int block_transfer(struct kshim_ufs_lun *l,uint64_t lba,uint32_t count,
                           void *data,bool write)
{
    if(!l||!data||!count||lba>=l->block.block_count||
       count>l->block.block_count-lba||
       (uint64_t)count*l->block.block_size>SIZE_MAX)return KSHIM_STORAGE_RANGE;
    uint8_t *p=data;
    for(uint32_t i=0;i<count;++i){
        uint8_t cdb[16]={0};cdb[0]=write?0x8a:0x88;cdb[1]=write?8:0;
        be64(cdb+2,lba+i);be32(cdb+10,1);
        int rc=scsi(l->host,l->number,cdb,p,l->block.block_size,write?1:2);
        if(rc)return rc;
        p+=l->block.block_size;
    }
    return 0;
}
static int block_read(void *l,uint64_t b,uint32_t n,void *p)
{ return block_transfer(l,b,n,p,false); }
static int block_write(void *l,uint64_t b,uint32_t n,const void *p)
{ return block_transfer(l,b,n,(void *)p,true); }
static int block_flush(void *ctx)
{
    struct kshim_ufs_lun *l=ctx;uint8_t cdb[16]={0};cdb[0]=0x35;
    return scsi(l->host,l->number,cdb,NULL,0,0);
}
int kshim_ufs_open_lun(struct kshim_ufs_host *h,uint8_t number,
                       bool allow_writes,struct kshim_ufs_lun *l)
{
    if(!h||!l||number>=8)return KSHIM_STORAGE_ARG;
    memset(l,0,sizeof(*l));
    uint8_t cdb[16]={0},data[32]={0};int rc;
    /* Autosense clears the initial UNIT ATTENTION; retry only that condition. */
    for(unsigned i=0;;++i){
        rc=scsi(h,number,cdb,NULL,0,0);
        if(!rc)break;
        if(i==2||rc!=KSHIM_UFS_SCSI||h->last_sense_key!=6)return rc;
    }
    cdb[0]=0x9e;cdb[1]=0x10;be32(cdb+10,32);
    rc=scsi(h,number,cdb,data,32,2);if(rc)return rc;
    uint64_t last=get64(data);uint32_t block_size=get32(data+8);
    if(last==UINT64_MAX||(block_size!=512&&block_size!=4096))
        return KSHIM_UFS_UNSUPPORTED;
    l->host=h;l->number=number;
    l->block=(struct kshim_blockdev){l,block_size,last+1,block_read,
                                   allow_writes?block_write:NULL,
                                   allow_writes?block_flush:NULL};
    return 0;
}
int kshim_ufs_release(struct kshim_ufs_host *h)
{
    if(!h)return KSHIM_STORAGE_ARG;
    if(!h->owned)return 0;
    /* A submitted UIC operation that did not conclusively complete leaves the
     * link in an unknown state: never restore firmware registers over it. */
    if(h->link.uncertain||h->power.uncertain)return KSHIM_UFS_QUARANTINED;
    /* SSU and its verification need our running transfer list. Restore the
     * device before stopping that list or returning the link to HIBERN8. */
    int rc=restore_power(h);if(rc)return rc;
    h->poisoned=1;uint32_t db,tdb;
    if((rc=wr(h,RUN,0))||(rc=wait_clear(h,RUN,1))||
       (rc=rd(h,DOORBELL,&db)))return rc;
    if(db)return KSHIM_UFS_BUSY;
    if(h->ops.manage_link){
        if((rc=rd(h,TASK_DB,&tdb)))return rc;
        if(tdb)return KSHIM_UFS_BUSY;
        /* Phases 4-5: put back the HIBERN8 entry state we exited, and only
         * drop the obligation once the returned TX state confirms it. */
        if(h->link.restore_hibern8){
            h->link.phase=4;
            if((rc=uic_cmd(h,UIC_HIBERN8_ENTER,0,IS_UIC|IS_ENTER,true)))
                return rc;
            h->link.phase=5;
            if((rc=uic_cmd(h,UIC_GET,MPHY_TX_FSM,IS_UIC,false)))return rc;
            if(h->link.current_tx!=TX_HIBERN8)return KSHIM_UFS_RESPONSE;
            h->link.restore_hibern8=0;
        }
    }
    /* No controller-owned descriptors remain. Restore the idle firmware list. */
    if((rc=wr(h,BASE_LO,h->saved_base[0]))||(rc=wr(h,BASE_HI,h->saved_base[1]))||
       (rc=wr(h,IS,1))||(rc=wr(h,AHIT,h->saved_ahit))||
       (rc=wr(h,RUN,h->saved_run))||(rc=wr(h,IE,h->saved_ie)))return rc;
    if(h->ops.manage_link)h->link.phase=6;
    h->owned=0;h->poisoned=0;return 0;
}
