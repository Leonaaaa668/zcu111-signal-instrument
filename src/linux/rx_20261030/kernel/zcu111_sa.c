// SPDX-License-Identifier: GPL-2.0-only
/* Same-XSA snapshot DMA client, Linux 6.12 / PetaLinux 2025.2 target.
 * This is not the clock/RFDC initialization service. No TX register writes.
 * The ordered Linux prepare service releases HOLD_RESET before late DMA probe.
 * Normal captures NEVER assert HOLD_RESET while Linux owns the DMA channel.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/completion.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/iopoll.h>
#include <linux/delay.h>
#include <linux/kref.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/unaligned.h>
#include <linux/capability.h>
#include <linux/math64.h>
#include "zcu111_sa_uapi.h"

#define MAX_PACKET (256U + 8U * 65536U)
#define RESULT_ID 0x31535852U
#define NCO_WORD 0x400000000000ULL
#define WAIT_US 3000000
struct zsa {
 struct miscdevice misc;
 struct mutex lock;
 struct kref refs;
 struct device *dev, *dma_dev;
 struct dma_chan *chan;
 void __iomem *result, *gpio, *rfdc;
 void *buffer;
 dma_addr_t dma_address;
 struct completion complete;
 enum dmaengine_tx_result dma_result;
 u32 residue, epoch, ticket, control, log_n;
 u64 center_hz, nco_word;
 u32 nco_update_seq;
 bool tuning;
 size_t bytes;
 bool opened, dead, faulted, pinned;
 struct zsa_receipt receipt;
};
static u32 word(struct zsa *s, unsigned off) { return readl(s->result + off); }
static void put(struct zsa *s, unsigned off, u32 val) { writel(val, s->result + off); }
static u64 raw_nco(struct zsa *s, unsigned slice)
{
 /* xrfdc_hw.h: ADC tile DRP=0x16000+tile*0x4000; block=slice*0x400. */
 void __iomem *p=s->rfdc + 0x1a000 + slice * 0x400;
 return ((u64)readw(p+0x94)<<32) | ((u64)readw(p+0x98)<<16) | readw(p+0x9c);
}
static bool nco_ok(struct zsa *s)
{
 return !s->tuning && raw_nco(s,2)==s->nco_word && raw_nco(s,3)==s->nco_word;
}
static void dma_done(void *context, const struct dmaengine_result *result)
{
 struct zsa *s=context;
 s->dma_result=result ? result->result : DMA_TRANS_ABORTED;
 s->residue=result ? result->residue : MAX_PACKET;
 complete(&s->complete);
}
static void stop_source(struct zsa *s)
{
 u32 old=readl(s->gpio);
 writel((old ^ BIT(31)) & 0x80071f00U, s->gpio);
 readl(s->gpio);
}
static int cancel_idle(struct zsa *s)
{
 u32 status;
 put(s,0,2); /* CANCEL, but HOLD_RESET remains zero. */
 return readl_poll_timeout(s->result+4,status,!(status & 0x0f02U),10,WAIT_US);
}
static int terminate(struct zsa *s)
{
 int ret=dmaengine_terminate_sync(s->chan);
 if (ret && !s->pinned) {
  /* Never free memory potentially still owned by a failed DMA termination.
   * Pin code/context until system restart; suppress sysfs bind/unbind below.
   */
  s->pinned=true; s->faulted=true;
  __module_get(THIS_MODULE); kref_get(&s->refs);
  dev_err(s->dev,"DMA termination failed; buffer/module retained; restart required\n");
 }
 return ret;
}
static u32 header(struct zsa *s, unsigned index)
{
 return get_unaligned_le32((u8 *)s->buffer+4*index);
}
static u64 join_header(struct zsa *s, unsigned index)
{
 return (u64)header(s,index) | ((u64)header(s,index+1)<<32);
}
static int validate(struct zsa *s, size_t bytes, u64 counter_epoch)
{
 u32 n=1U<<s->log_n,k;
 u64 xor=0,seq=join_header(s,5);
 if (header(s,0)!=0x31504153 || header(s,1)!=1 || header(s,2)!=256 ||
     header(s,3)!=bytes || header(s,4)!=0x3f || !seq ||
     header(s,7)!=s->epoch || header(s,8)!=0 || header(s,9)!=s->nco_update_seq ||
     header(s,10)!=s->ticket || header(s,11)!=n || header(s,12)!=s->log_n ||
     header(s,13)!=0 || header(s,14)!=s->log_n+1 || header(s,15)!=1 ||
     header(s,16)!=2400000000U || header(s,17)!=1200000000U ||
     header(s,18)!=150000000U || header(s,19)!=1200000000U ||
     join_header(s,20)!=s->center_hz || join_header(s,22)!=s->nco_word ||
     join_header(s,30)!=counter_epoch || header(s,32) || header(s,37)!=1 ||
     (header(s,36)&0x801fffffU)!=((s->control&BIT(31))|(s->log_n<<13)|15U) ||
     (header(s,36)&0x7fe00000U) || header(s,38) || header(s,39) ||
     header(s,44)!=17 || header(s,45)!=8 || header(s,46)!=24 ||
     header(s,47)!=16 || header(s,48)!=1 || header(s,49)!=0)
  return -EBADMSG;
 for (k=50;k<56;k++) if (header(s,k)) return -EBADMSG;
 for (k=0;k<n;k++) {
  u64 p=get_unaligned_le64((u8 *)s->buffer+256+8*k);
  if (p>BIT_ULL(47)) return -EBADMSG;
  xor^=p;
 }
 if (xor!=join_header(s,34) || join_header(s,28)<join_header(s,26) ||
     ((join_header(s,28)-join_header(s,26))&3)) return -EBADMSG;
 if (word(s,12)!=1 || word(s,16)!=s->ticket || word(s,32)!=s->epoch ||
     ((u64)word(s,24)|((u64)word(s,28)<<32))!=seq) return -ESTALE;
 s->receipt=(struct zsa_receipt){.ticket_id=s->ticket,.epoch=s->epoch,
  .nco_update_seq=s->nco_update_seq,.log_n=s->log_n,.packet_sequence=seq,.counter_epoch=counter_epoch};
 return 0;
}
static int capture(struct zsa *s)
{
 struct dma_async_tx_descriptor *desc;
 dma_cookie_t cookie;
 u32 status,meta[16],k;
 u64 counter_epoch=ktime_get_ns();
 size_t bytes=256U+(8U<<s->log_n);
 int ret;
 bool submitted=false;
 if (!nco_ok(s)) return -EPIPE;
 if (word(s,0)&1) return -EHOSTDOWN;
 s->bytes=0;
 ret=cancel_idle(s); if (ret) goto fail;
 put(s,0,0); /* Clear CANCEL without resetting the Linux-owned DMA. */
 if (++s->epoch==0) ++s->epoch;
 s->ticket=get_random_u32(); if (!s->ticket) s->ticket=1;
 s->control=((readl(s->gpio)^BIT(31))&BIT(31)) | (s->log_n<<8) | 1;
 writel(s->control,s->gpio);
 ret=readl_poll_timeout(s->gpio+8,status,
  (status&0x801fffffU)==((s->control&BIT(31))|(s->log_n<<13)|15U),10,WAIT_US);
 if (ret) goto fail;
 memset(s->buffer,0,bytes);
 reinit_completion(&s->complete);
 s->dma_result=DMA_TRANS_ABORTED; s->residue=bytes;
 desc=dmaengine_prep_slave_single(s->chan,s->dma_address,bytes,DMA_DEV_TO_MEM,
                                  DMA_PREP_INTERRUPT|DMA_CTRL_ACK);
 if (!desc) { ret=-EIO; goto fail; }
 desc->callback_result=dma_done; desc->callback_param=s;
 dma_wmb(); cookie=dmaengine_submit(desc); submitted=true; ret=dma_submit_error(cookie);
 if (ret) goto fail;
 dma_async_issue_pending(s->chan);
 memset(meta,0,sizeof(meta));
 meta[0]=s->epoch;meta[2]=s->nco_update_seq;meta[3]=s->ticket;
 meta[4]=lower_32_bits(s->center_hz);meta[5]=upper_32_bits(s->center_hz);
 meta[6]=lower_32_bits(s->nco_word);meta[7]=upper_32_bits(s->nco_word);
 meta[8]=lower_32_bits(counter_epoch);meta[9]=upper_32_bits(counter_epoch);
 meta[10]=2400000000U;meta[11]=1200000000U;meta[12]=1;meta[13]=s->control;meta[15]=0x52585331;
 for (k=0;k<16;k++) put(s,0x100+4*k,meta[k]);
 wmb();put(s,0,256);
 if (!wait_for_completion_timeout(&s->complete,msecs_to_jiffies(3000))) { ret=-ETIMEDOUT;goto fail; }
 dmaengine_synchronize(s->chan);
 if (s->dma_result!=DMA_TRANS_NOERROR || s->residue) {ret=-EIO;goto fail;}
 ret=readl_poll_timeout(s->result+4,status,(status&4),10,WAIT_US);
 if (ret) goto fail;
 dma_rmb();
 if (!nco_ok(s)) {ret=-ESTALE;goto fail;}
 ret=validate(s,bytes,counter_epoch);if(ret)goto fail;
 ret=cancel_idle(s);if(ret)goto fail;
 stop_source(s);put(s,0,0);s->bytes=bytes;
 return 0;
fail:
 put(s,0,2);
 if (submitted && terminate(s)) return -EIO;
 stop_source(s);
 /* Partial AXIS/FIFO content is never reused. Restart the device before a
  * retry after an issued-DMA failure; do not blindly reset a bound DMA core.
  */
 if (submitted) s->faulted=true;
 return ret;
}
static void destroy(struct kref *ref) { kfree(container_of(ref,struct zsa,refs)); }
static int open_file(struct inode *inode,struct file *file)
{
 struct zsa *s=container_of(file->private_data,struct zsa,misc);
 int ret=0;
 mutex_lock(&s->lock);
 if(s->dead)ret=-ENODEV;
 else if(s->opened)ret=-EBUSY;
 else {s->opened=true;s->bytes=0;kref_get(&s->refs);file->private_data=s;ret=nonseekable_open(inode,file);}
 mutex_unlock(&s->lock);return ret;
}
static int close_file(struct inode *inode,struct file *file)
{
 struct zsa *s=file->private_data;
 mutex_lock(&s->lock);s->opened=false;s->bytes=0;mutex_unlock(&s->lock);
 kref_put(&s->refs,destroy);return 0;
}
static ssize_t read_file(struct file *file,char __user *buffer,size_t count,loff_t *pos)
{
 struct zsa *s=file->private_data;ssize_t ret;
 if (!count) return 0;
 if (mutex_lock_interruptible(&s->lock)) return -ERESTARTSYS;
 if(s->dead)ret=-ENODEV;
 else if(s->faulted)ret=-EIO;
 else {
  ret=0;if(!s->bytes && !*pos)ret=capture(s);
  if(!ret)ret=simple_read_from_buffer(buffer,count,pos,s->buffer,s->bytes);
 }
 mutex_unlock(&s->lock);return ret;
}
static long ioctl_file(struct file *file,unsigned int cmd,unsigned long arg)
{
 struct zsa *s=file->private_data;struct zsa_config cfg;int ret=0;
 struct zsa_tune_state tune;
 struct zsa_config_v2 cfg2;
 u64 expected,actual;
 if(mutex_lock_interruptible(&s->lock))return -ERESTARTSYS;
 if(s->dead){ret=-ENODEV;goto out;}
 if(s->faulted){ret=-EIO;goto out;}
 if(cmd==ZSA_IOC_TUNE_STATE){
  tune=(struct zsa_tune_state){.center_hz=s->center_hz,.nco_word=s->nco_word,
   .nco_update_seq=s->nco_update_seq,.flags=s->tuning?ZSA_TUNE_IN_PROGRESS:(nco_ok(s)?ZSA_TUNE_VALID:0)};
  if(copy_to_user((void __user *)arg,&tune,sizeof(tune)))ret=-EFAULT;
 }else if(cmd==ZSA_IOC_TUNE_BEGIN){
  if(!capable(CAP_SYS_RAWIO)){ret=-EPERM;goto out;}
  s->tuning=true;s->bytes=0;file->f_pos=0;
  ret=cancel_idle(s);
  if(!ret)stop_source(s);
 }else if(cmd==ZSA_IOC_TUNE_COMMIT){
  if(!capable(CAP_SYS_RAWIO)){ret=-EPERM;goto out;}
  if(!s->tuning){ret=-EINVAL;goto out;}
  if(copy_from_user(&tune,(void __user *)arg,sizeof(tune))){ret=-EFAULT;goto out;}
  if(tune.center_hz<1300000000ULL || tune.center_hz>2300000000ULL || tune.flags || tune.nco_update_seq || tune.nco_word){ret=-EINVAL;goto out;}
  /* Positive RF frequency in Nyquist zone2 maps to +(Fs-center) digitally.
   * Match official RFDC rounding within two FCW LSBs; store actual readback. */
  expected=mul_u64_u64_div_u64(2400000000ULL-tune.center_hz,BIT_ULL(48),2400000000ULL);
  actual=raw_nco(s,2);
  if(actual!=raw_nco(s,3) || actual+2<expected || actual>expected+2){ret=-EIO;goto out;}
  usleep_range(2000,3000);
  if(actual!=raw_nco(s,2) || actual!=raw_nco(s,3)){ret=-ESTALE;goto out;}
  s->center_hz=tune.center_hz;s->nco_word=actual;
  if(++s->nco_update_seq==0)++s->nco_update_seq;
  s->tuning=false;
  tune=(struct zsa_tune_state){.center_hz=s->center_hz,.nco_word=actual,.nco_update_seq=s->nco_update_seq,.flags=ZSA_TUNE_VALID};
  if(copy_to_user((void __user *)arg,&tune,sizeof(tune)))ret=-EFAULT;
 }else if(cmd==ZSA_IOC_CONFIG_V2){
  if(copy_from_user(&cfg2,(void __user *)arg,sizeof(cfg2))){ret=-EFAULT;goto out;}
  if((cfg2.log_n!=10 && cfg2.log_n!=13 && cfg2.log_n!=16) || cfg2.center_hz!=s->center_hz || cfg2.nco_update_seq!=s->nco_update_seq || !nco_ok(s)){ret=-ESTALE;goto out;}
  s->log_n=cfg2.log_n;s->bytes=0;file->f_pos=0;
 }else if(cmd==ZSA_IOC_CONFIG){
  if(s->center_hz!=1800000000ULL || !nco_ok(s)){ret=-EPIPE;goto out;}
  if(copy_from_user(&cfg,(void __user *)arg,sizeof(cfg))){ret=-EFAULT;goto out;}
  if(cfg.reserved || (cfg.log_n!=10 && cfg.log_n!=13 && cfg.log_n!=16)){ret=-EINVAL;goto out;}
  s->log_n=cfg.log_n;s->bytes=0;file->f_pos=0;
 }else if(cmd==ZSA_IOC_RECEIPT){
  if(!s->bytes)ret=-ENODATA;
  else if(copy_to_user((void __user *)arg,&s->receipt,sizeof(s->receipt)))ret=-EFAULT;
 }else ret=-ENOTTY;
out:mutex_unlock(&s->lock);return ret;
}
static const struct file_operations fops={.owner=THIS_MODULE,.open=open_file,.release=close_file,
 .read=read_file,.unlocked_ioctl=ioctl_file};
static int probe(struct platform_device *pdev)
{
 struct zsa *s;struct device_node *node;struct resource resource;struct clk *clk;int ret;
 BUILD_BUG_ON(sizeof(struct zsa_receipt)!=40);
 s=kzalloc(sizeof(*s),GFP_KERNEL);if(!s)return -ENOMEM;
 s->center_hz=1800000000ULL;s->nco_word=NCO_WORD;s->nco_update_seq=1;
 mutex_init(&s->lock);kref_init(&s->refs);init_completion(&s->complete);s->dev=&pdev->dev;s->log_n=10;
 clk=devm_clk_get_enabled(s->dev,"ctrl_clk");
 if(IS_ERR(clk)){ret=PTR_ERR(clk);goto free;}
 s->result=devm_platform_ioremap_resource_byname(pdev,"result");
 if(IS_ERR(s->result)){ret=PTR_ERR(s->result);goto free;}
 s->gpio=devm_platform_ioremap_resource_byname(pdev,"control");
 if(IS_ERR(s->gpio)){ret=PTR_ERR(s->gpio);goto free;}
 if(word(s,8)!=RESULT_ID){ret=-ENODEV;goto free;}
 if(word(s,0)&1){dev_err(s->dev,"Release HOLD_RESET before AXI DMA driver probe (see README)\n");ret=-EHOSTDOWN;goto free;}
 writel(0,s->gpio+4); /* GPIO channel1 controls are outputs. */
 writel(0xffffffffU,s->gpio+12); /* Channel2 is DSP status input. */
 node=of_parse_phandle(pdev->dev.of_node,"rfdc",0);
 if(!node){ret=-EINVAL;goto free;}
 ret=of_address_to_resource(node,0,&resource);of_node_put(node);if(ret)goto free;
 if(resource_size(&resource)<0x1ad00){ret=-EINVAL;goto free;}
 /* Read-only RFDC NCO view; Linux RFDC initialization retains configuration ownership. */
 s->rfdc=devm_ioremap(s->dev,resource.start,resource_size(&resource));
 if(!s->rfdc){ret=-ENOMEM;goto free;}
 s->chan=dma_request_chan(s->dev,"rx");if(IS_ERR(s->chan)){ret=PTR_ERR(s->chan);goto free;}
 s->dma_dev=dmaengine_get_dma_device(s->chan);
 /* The frozen HP1 route reaches only the low 2GiB of PS DDR, including
  * the generic AXI DMA driver's SG descriptors. Require the DT bus limit
  * before accepting buffers; do not silently rely on a 40-bit mask. */
 if(!s->dma_dev->bus_dma_limit || s->dma_dev->bus_dma_limit>0x7fffffffULL){
  dev_err(s->dev,"RX requires a low-2GiB dma-ranges limit on amba_pl\n");ret=-ERANGE;goto channel;
 }
 s->buffer=dma_alloc_coherent(s->dma_dev,MAX_PACKET,&s->dma_address,GFP_KERNEL);
 if(!s->buffer){ret=-ENOMEM;goto channel;}
 if((u64)s->dma_address+MAX_PACKET>0x80000000ULL){ret=-ERANGE;goto buffer;}
 dev_info(s->dev,"RX coherent buffer=%pad, bus limit=%#llx\n",&s->dma_address,(unsigned long long)s->dma_dev->bus_dma_limit);
 s->misc=(struct miscdevice){.minor=MISC_DYNAMIC_MINOR,.name="zcu111_spectrum",.fops=&fops,.parent=s->dev,.mode=0600};
 ret=misc_register(&s->misc);if(ret)goto buffer;
 platform_set_drvdata(pdev,s);dev_info(s->dev,"Spectrum snapshot client ready; no automatic RX/TX\n");return 0;
buffer:dma_free_coherent(s->dma_dev,MAX_PACKET,s->buffer,s->dma_address);
channel:dma_release_channel(s->chan);
free:kref_put(&s->refs,destroy);return ret;
}
static void remove_driver(struct platform_device *pdev)
{
 struct zsa *s=platform_get_drvdata(pdev);
 misc_deregister(&s->misc);mutex_lock(&s->lock);s->dead=true;
 put(s,0,2);
 if(!terminate(s)) {
  stop_source(s);
  dma_free_coherent(s->dma_dev,MAX_PACKET,s->buffer,s->dma_address);
  dma_release_channel(s->chan);
 }
 mutex_unlock(&s->lock);kref_put(&s->refs,destroy);
}
static const struct of_device_id matches[]={{.compatible="zcu111,spectrum-snapshot-v1"},{}};
MODULE_DEVICE_TABLE(of,matches);
static struct platform_driver zsa_driver={.probe=probe,.remove=remove_driver,
 .driver={.name="zcu111-spectrum",.of_match_table=matches,.suppress_bind_attrs=true}};
module_platform_driver(zsa_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ZCU111 same-XSA one-shot full spectrum DMA client");
