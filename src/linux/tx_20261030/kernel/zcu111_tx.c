// SPDX-License-Identifier: GPL-2.0-only
/* Fixed-XSA Linux TX owner. Polling CDMA plus autonomous cyclic MM2S.
 * No fixed PS RAM, /dev/mem, JTAG, IRQ assumptions, or automatic RF playback.
 * Resources must be free: never share registers with xilinx_dma/gpio drivers.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/dma-mapping.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/capability.h>
#include <linux/clk.h>
#include <linux/of_clk.h>
#include <linux/of_address.h>
#include <linux/gpio.h>
#include <linux/gpio/driver.h>
#include <crypto/hash.h>
#include "zcu111_tx_uapi.h"
#define WAVE 0x500100000ULL
#define CHUNK (4U*1024U*1024U)
#define TIMEOUT_US 3000000
#define CR 0x00
#define SR 0x04
#define CDMA_ERR 0x770U
#define MM2S_ERR 0x770U
#define RESET 4U
struct ztx_bd { __le32 next, next_hi, buf, buf_hi, rsv[2], control, status, app[5], pad[3]; };
static_assert(sizeof(struct ztx_bd)==64);
/* PS buffers/BDs must stay within the frozen HP0 low-2GiB aperture.
 * The PL DDR target itself still uses 40-bit DMA address registers. */
#define PS_DMA_MASK DMA_BIT_MASK(32)
#define PS_APERTURE_LIMIT 0x80000000ULL
struct ztx {
 struct miscdevice misc; struct device *dev; struct mutex lock;
 void __iomem *copy,*play,*gpio,*rf;
 void *stage,*back; dma_addr_t stage_dma,back_dma;
 struct ztx_bd *bd; dma_addr_t bd_dma;
 struct crypto_shash *sha; struct shash_desc *hash;
 struct delayed_work monitor;
 u64 expected,received,loaded,session; u8 digest[32];
 bool opened,loading,playing,prepared,faulted,pinned,dma_active; int error;
};
static bool manual_bind;
static bool clock_only;
static bool pulse_fabric_reset;
module_param(pulse_fabric_reset,bool,0400);
MODULE_PARM_DESC(pulse_fabric_reset,"Fixed-XSA stopped bring-up: pulse PS GPIO EMIO95 only after acquiring the control clock; resets PL datapaths and DDR calibration");
static bool allow_dma_mmio;
module_param(allow_dma_mmio,bool,0400);
MODULE_PARM_DESC(allow_dma_mmio,"Explicit bring-up opt-in after clock/reset validation; default prohibits all DMA register access");
module_param(clock_only,bool,0400);
MODULE_PARM_DESC(clock_only,"Acquire only the AXI control clock, without mapping or accessing PL registers");
module_param(manual_bind,bool,0400);
MODULE_PARM_DESC(manual_bind,"Explicit fixed-ZCU111 bootstrap binding; all three resources must be unowned");
static struct platform_device *manual_device;
static u64 manual_dma_mask=PS_DMA_MASK;
static u32 ztx_guard(struct ztx *t){return readl(t->gpio+8);}
static u64 nco(struct ztx *t){return ((u64)readw(t->rf+0xa094)<<32)|((u64)readw(t->rf+0xa098)<<16)|readw(t->rf+0xa09c);}
static bool ready(struct ztx *t){return (ztx_guard(t)&0x87)==0x87 && readw(t->rf+0x800c)==15 && nco(t)==0x400000000000ULL;}
static void fail(struct ztx *t,int err){writel(0,t->gpio);readl(t->gpio);t->playing=false;t->loading=false;t->loaded=0;t->faulted=true;t->error=err;}
static int reset_ip(struct ztx *t,void __iomem *base){u32 val;writel(RESET,base+CR);return readl_poll_timeout(base+CR,val,!(val&RESET),10,TIMEOUT_US);}
static void pin(struct ztx *t){if(!t->pinned){t->pinned=true;__module_get(THIS_MODULE);dev_err(t->dev,"DMA reset failed; buffers and module pinned until reboot\n");}}
static int stop(struct ztx *t){
 int ret;writel(0,t->gpio);readl(t->gpio);t->playing=false;usleep_range(10,30);
 if(!t->dma_active)return 0;
 ret=reset_ip(t,t->play);if(ret){fail(t,ret);pin(t);return ret;}
 writel(2,t->gpio);readl(t->gpio);usleep_range(100,150);writel(0,t->gpio);readl(t->gpio);usleep_range(100,150);return 0;
}
static int copy(struct ztx *t,u64 src,u64 dst,u32 bytes){
 u32 val;int ret;
 if(!bytes||bytes>CHUNK||(bytes&63)||(src&63)||(dst&63))return -EINVAL;
 writel(~0U,t->copy+SR);writel(lower_32_bits(src),t->copy+0x18);writel(upper_32_bits(src),t->copy+0x1c);
 writel(lower_32_bits(dst),t->copy+0x20);writel(upper_32_bits(dst),t->copy+0x24);dma_wmb();writel(bytes,t->copy+0x28);
 ret=readl_poll_timeout(t->copy+SR,val,(val&(BIT(1)|CDMA_ERR)),10,TIMEOUT_US);
 if(ret||(val&CDMA_ERR)){
  int r=reset_ip(t,t->copy);if(r)pin(t);fail(t,ret?ret:-EIO);return ret?ret:-EIO;
 }
 dma_rmb();return 0;
}
static void monitor(struct work_struct *work){
 struct ztx *t=container_of(to_delayed_work(work),struct ztx,monitor);
 mutex_lock(&t->lock);
 if(t->playing){
  if((ztx_guard(t)&0xf7)!=0x97 || (readl(t->play+SR)&MM2S_ERR) || !ready(t)){
   fail(t,-EIO);stop(t);
  }else schedule_delayed_work(&t->monitor,msecs_to_jiffies(20));
 }
 mutex_unlock(&t->lock);
}
static int open_tx(struct inode *ino,struct file *file){
 struct ztx *t=container_of(file->private_data,struct ztx,misc);int ret=0;
 stream_open(ino,file);
 if(!(file->f_mode&FMODE_WRITE)){file->private_data=t;return 0;}
 mutex_lock(&t->lock);if(t->opened)ret=-EBUSY;else{t->opened=true;file->private_data=t;}
 mutex_unlock(&t->lock);return ret;
}
static int release_tx(struct inode *ino,struct file *file){
 struct ztx *t=file->private_data;
 if(!(file->f_mode&FMODE_WRITE))return 0;
 mutex_lock(&t->lock);
 /* Close is always a stop. A client cannot orphan a running transmitter. */
 if(!t->pinned)stop(t);else writel(0,t->gpio);
 t->opened=false;t->loading=false;t->loaded=0;
 mutex_unlock(&t->lock);cancel_delayed_work_sync(&t->monitor);return 0;
}
static ssize_t write_tx(struct file *file,const char __user *data,size_t count,loff_t *offset){
 struct ztx *t=file->private_data;int ret;size_t done=0;
 if(!count||count%256)return -EINVAL;
 mutex_lock(&t->lock);
 if(!t->loading||t->faulted){ret=-EPIPE;goto out;}
 if(count>t->expected-t->received){ret=-EFBIG;goto out;}
 while(done<count){u32 n=min_t(size_t,count-done,CHUNK);
  if(copy_from_user(t->stage,data+done,n)){ret=-EFAULT;goto abort;}
  ret=copy(t,t->stage_dma,WAVE+t->received,n);if(ret)goto abort;
  ret=copy(t,WAVE+t->received,t->back_dma,n);if(ret)goto abort;
  if(memcmp(t->stage,t->back,n)){ret=-EIO;goto abort;}
  ret=crypto_shash_update(t->hash,t->stage,n);if(ret)goto abort;
  t->received+=n;done+=n;
 }
 ret=done;goto out;
abort:fail(t,ret);
out:mutex_unlock(&t->lock);return ret;
}
static int start(struct ztx *t){
 u32 chunk,status;unsigned i;int ret;
 if(!t->prepared||!ready(t))return -EAGAIN;
 if(t->faulted||!t->loaded||t->loading)return -ENODATA;
 ret=stop(t);if(ret)return ret;
 memset(t->bd,0,PAGE_SIZE);chunk=t->loaded/4;
 for(i=0;i<4;i++){
  u64 next=t->bd_dma+((i+1)%4)*64,buf=WAVE+(u64)i*chunk;
  t->bd[i].next=cpu_to_le32(lower_32_bits(next));t->bd[i].next_hi=cpu_to_le32(upper_32_bits(next));
  t->bd[i].buf=cpu_to_le32(lower_32_bits(buf));t->bd[i].buf_hi=cpu_to_le32(upper_32_bits(buf));
  t->bd[i].control=cpu_to_le32(chunk|0x0c000000U);
 }
 dma_wmb();writel(lower_32_bits(t->bd_dma),t->play+8);writel(upper_32_bits(t->bd_dma),t->play+12);
 writel(BIT(4)|BIT(0),t->play+CR);
 /* A dummy tail outside the cyclic ring triggers startup and is never reached. */
 writel(upper_32_bits(t->bd_dma+256),t->play+0x14);writel(lower_32_bits(t->bd_dma+256),t->play+0x10);
 usleep_range(1000,1500);
 if(readl(t->play+SR)&MM2S_ERR){ret=-EIO;goto bad;}
 writel(1,t->gpio);readl(t->gpio);
 ret=readl_poll_timeout(t->gpio+8,status,(status&0xf7)==0x97||(status&0x60),20,100000);
 if(ret||(status&0xf7)!=0x97){ret=ret?ret:-EIO;goto bad;}
 t->playing=true;schedule_delayed_work(&t->monitor,msecs_to_jiffies(20));return 0;
bad:fail(t,ret);stop(t);return ret;
}
static long ioctl_tx(struct file *file,unsigned cmd,unsigned long arg){
 struct ztx *t=file->private_data;int ret=0;
 if(!(file->f_mode&FMODE_WRITE)&&cmd!=ZTX_IOC_INFO)return -EPERM;
 mutex_lock(&t->lock);
 if(cmd==ZTX_IOC_INFO){
  struct ztx_info i={.abi_version=ZTX_ABI_VERSION,.struct_bytes=sizeof(i),.max_bytes=ZTX_MAX_BYTES};
  i.flags=(t->prepared&&ready(t)?ZTX_F_PREPARED:0)|(t->loaded?ZTX_F_LOADED:0)|(t->playing?ZTX_F_PLAYING:0)|(t->loading?ZTX_F_LOADING:0)|(t->faulted?ZTX_F_FAULT:0);
  i.guard_status=ztx_guard(t);i.cdma_status=t->dma_active?readl(t->copy+SR):0;i.mm2s_status=t->dma_active?readl(t->play+SR):0;i.dac_state=readw(t->rf+0x800c);i.last_errno=t->error;
  i.loaded_bytes=t->loaded;i.received_bytes=t->received;i.session=t->session;i.nco_word=nco(t);
  if(copy_to_user((void __user *)arg,&i,sizeof(i)))ret=-EFAULT;
 }else if(cmd==ZTX_IOC_DIAG){
  struct ztx_dma_diag d;
  if(!allow_dma_mmio){ret=-EOPNOTSUPP;goto out;}
  if(!capable(CAP_SYS_RAWIO)){ret=-EPERM;goto out;}
  if(t->playing||t->loading){ret=-EBUSY;goto out;}
  if(!(ztx_guard(t)&BIT(1))){ret=-EAGAIN;goto out;}
  dev_info(t->dev,"diagnostic: before CDMA status read (0xb0080004), no reset\n");
  d.cdma_status=readl(t->copy+SR);
  dev_info(t->dev,"diagnostic: CDMA status=%#x; before MM2S read (0xb0090004)\n",d.cdma_status);
  d.mm2s_status=readl(t->play+SR);
  dev_info(t->dev,"diagnostic: MM2S status=%#x; no register writes performed\n",d.mm2s_status);
  if(copy_to_user((void __user *)arg,&d,sizeof(d)))ret=-EFAULT;
 }else if(cmd==ZTX_IOC_PREPARED){
  if(!allow_dma_mmio)ret=-EOPNOTSUPP;
  else if(!capable(CAP_SYS_RAWIO))ret=-EPERM;
  else if(t->playing||t->loading)ret=-EBUSY;
  else if(!ready(t))ret=-EAGAIN;
  else {
   dev_info(t->dev,"prepare: resetting CDMA at 0xb0080000\n");
   ret=reset_ip(t,t->copy);
   dev_info(t->dev,"prepare: CDMA reset returned %d\n",ret);
   if(!ret){dev_info(t->dev,"prepare: resetting MM2S at 0xb0090000\n");ret=reset_ip(t,t->play);dev_info(t->dev,"prepare: MM2S reset returned %d\n",ret);}
   if(!ret){t->dma_active=true;t->prepared=true;}else fail(t,ret);
  }
 }else if(cmd==ZTX_IOC_STOP||cmd==ZTX_IOC_ABORT){
  if(!t->pinned)ret=stop(t);else ret=-EIO;
  if(cmd==ZTX_IOC_ABORT){t->loading=false;t->loaded=0;}
 }else if(cmd==ZTX_IOC_BEGIN){
  struct ztx_begin b;
  if(copy_from_user(&b,(void __user *)arg,sizeof(b))){ret=-EFAULT;goto out;}
  if(memchr_inv(b.reserved,0,sizeof(b.reserved))||!b.bytes||b.bytes>ZTX_MAX_BYTES||b.bytes%256){ret=-EINVAL;goto out;}
  if(t->pinned){ret=-EIO;goto out;}
  if(!t->prepared||!ready(t)){ret=-EAGAIN;goto out;}
  ret=stop(t);if(ret)goto out;
  ret=reset_ip(t,t->copy);if(ret){fail(t,ret);pin(t);goto out;}
  t->loaded=0;t->received=0;t->loading=false;t->faulted=false;t->error=0;
  ret=crypto_shash_init(t->hash);if(ret)goto out;
  t->expected=b.bytes;memcpy(t->digest,b.sha256,32);t->loading=true;t->session++;
 }else if(cmd==ZTX_IOC_COMMIT){
  u8 digest[32];
  if(!t->loading||t->received!=t->expected||t->faulted){ret=-EINVAL;goto out;}
  ret=crypto_shash_final(t->hash,digest);
  if(ret||memcmp(digest,t->digest,32)){ret=ret?ret:-EBADMSG;fail(t,ret);goto out;}
  t->loaded=t->expected;t->loading=false;
 }else if(cmd==ZTX_IOC_PLAY)ret=start(t);
 else ret=-ENOTTY;
out:mutex_unlock(&t->lock);return ret;
}
static const struct file_operations ops={.owner=THIS_MODULE,.open=open_tx,.release=release_tx,.write=write_tx,.unlocked_ioctl=ioctl_tx,.compat_ioctl=compat_ptr_ioctl,.llseek=NULL};
static void release_control_clock(void *data){struct clk *clock=data;clk_disable_unprepare(clock);clk_put(clock);}
static int control_clock(struct device *dev){
 struct device_node *node;struct resource r;struct clk *clock=NULL;int ret;
 /* A synthetic bootstrap device has no DT clocks. Keep a real reference to
  * the original TX GPIO's PS FCLK before touching any AXI register. */
 for_each_compatible_node(node,NULL,"xlnx,axi-gpio-2.0"){
  if(!of_address_to_resource(node,0,&r)&&r.start==0xb00a0000){clock=of_clk_get(node,0);of_node_put(node);break;}
 }
 if(!clock)return -ENODEV;
 if(IS_ERR(clock))return PTR_ERR(clock);
 ret=clk_prepare_enable(clock);if(ret){clk_put(clock);return ret;}
 return devm_add_action_or_reset(dev,release_control_clock,clock);
}
static int match_ps_gpio(struct gpio_chip *chip,const void *data){
 struct resource r;
 return chip->ngpio>173&&chip->parent&&chip->parent->of_node&&
  !of_address_to_resource(chip->parent->of_node,0,&r)&&r.start==0xff0a0000;
}
static int probe(struct platform_device *pdev){
 struct ztx *t;struct resource *r;int ret;
 t=devm_kzalloc(&pdev->dev,sizeof(*t),GFP_KERNEL);if(!t)return -ENOMEM;t->dev=&pdev->dev;
 ret=control_clock(t->dev);if(ret)return dev_err_probe(t->dev,ret,"Cannot own TX AXI control clock\n");
 dev_info(t->dev,"bootstrap: AXI control clock acquired\n");
 if(pulse_fabric_reset){
  struct gpio_device *gdev=gpio_device_find(NULL,match_ps_gpio);
  int reset_gpio;
  /* Frozen XSA psu_init.c drives GPIO bank5 bit31 = EMIO95 = line173.
   * Linux gpiolib owns the PS controller; never remap its registers or
   * toggle the different PMU RESET_PS_PL0 API, which firmware rejects. */
  if(!gdev)return -ENODEV;
  reset_gpio=gpio_device_get_base(gdev);
  if(reset_gpio<0){gpio_device_put(gdev);return -ENODEV;}
  reset_gpio+=173;
  ret=devm_gpio_request_one(t->dev,reset_gpio,GPIOF_OUT_INIT_HIGH,"zcu111-pl-resetn0");
  gpio_device_put(gdev);
  if(ret)return dev_err_probe(t->dev,ret,"Cannot own PS EMIO95 fabric reset\n");
  dev_info(t->dev,"bootstrap: asserting GPIO EMIO95 pl_resetn0 with control clock held\n");
  gpio_set_value_cansleep(reset_gpio,0);usleep_range(1000,1500);
  gpio_set_value_cansleep(reset_gpio,1);msleep(100);
  dev_info(t->dev,"bootstrap: GPIO pl_resetn0 released, waiting for DDR calibration\n");
 }
 mutex_init(&t->lock);INIT_DELAYED_WORK(&t->monitor,monitor);
 if(clock_only){platform_set_drvdata(pdev,t);dev_info(t->dev,"clock-only mode: no PL register accesses\n");return 0;}
 ret=dma_set_mask_and_coherent(t->dev,PS_DMA_MASK);if(ret)return ret;
 t->copy=devm_platform_ioremap_resource_byname(pdev,"upload");if(IS_ERR(t->copy))return PTR_ERR(t->copy);
 t->play=devm_platform_ioremap_resource_byname(pdev,"playback");if(IS_ERR(t->play))return PTR_ERR(t->play);
 t->gpio=devm_platform_ioremap_resource_byname(pdev,"control");if(IS_ERR(t->gpio))return PTR_ERR(t->gpio);
 /* RFDC is a shared read-only observer here; init is owned by its UIO service. */
 r=platform_get_resource_byname(pdev,IORESOURCE_MEM,"rfdc");if(!r)return -EINVAL;
 t->rf=devm_ioremap(t->dev,r->start,resource_size(r));if(!t->rf)return -ENOMEM;
 dev_info(t->dev,"bootstrap: mappings established, muting GPIO\n");
 writel(0,t->gpio);readl(t->gpio);
 dev_info(t->dev,"bootstrap: GPIO responsive, status=%#x\n",ztx_guard(t));
 {
  u32 status;
  ret=readl_poll_timeout(t->gpio+8,status,status&BIT(1),1000,5000000);
  if(ret)return dev_err_probe(t->dev,ret,"DDR calibration not ready; DMA MMIO will not be accessed\n");
 }
 t->stage=dma_alloc_coherent(t->dev,CHUNK,&t->stage_dma,GFP_KERNEL);if(!t->stage)return -ENOMEM;
 t->back=dma_alloc_coherent(t->dev,CHUNK,&t->back_dma,GFP_KERNEL);if(!t->back){ret=-ENOMEM;goto free_stage;}
 t->bd=dma_alloc_coherent(t->dev,PAGE_SIZE,&t->bd_dma,GFP_KERNEL);if(!t->bd){ret=-ENOMEM;goto free_back;}
 /* Linux ARM64 requires a 32-bit DMA zone mask. The frozen AXI route
  * is narrower: reject any allocation outside its low-2GiB aperture. */
 if((u64)t->stage_dma+CHUNK>PS_APERTURE_LIMIT ||
    (u64)t->back_dma+CHUNK>PS_APERTURE_LIMIT ||
    (u64)t->bd_dma+PAGE_SIZE>PS_APERTURE_LIMIT){
  ret=-ERANGE;dev_err(t->dev,"PS DMA buffer outside frozen HP0 low-2GiB route\n");goto free_bd;
 }
 dev_info(t->dev,"PS DMA buffers: stage=%pad back=%pad descriptors=%pad\n",&t->stage_dma,&t->back_dma,&t->bd_dma);
 t->sha=crypto_alloc_shash("sha256",0,0);if(IS_ERR(t->sha)){ret=PTR_ERR(t->sha);goto free_bd;}
 t->hash=kzalloc(sizeof(*t->hash)+crypto_shash_descsize(t->sha),GFP_KERNEL);if(!t->hash){ret=-ENOMEM;goto free_sha;}t->hash->tfm=t->sha;
 /* DMA MMIO remains untouched until root validates clocks/RFDC with PREPARED. */
 t->misc=(struct miscdevice){.minor=MISC_DYNAMIC_MINOR,.name="zcu111_tx",.fops=&ops,.parent=t->dev,.mode=0600};
 platform_set_drvdata(pdev,t);ret=misc_register(&t->misc);if(ret)goto free_hash;
 dev_info(t->dev,"TX ABI1 ready; default muted, no waveform loaded\n");return 0;
free_hash:kfree(t->hash);
free_sha:crypto_free_shash(t->sha);
free_bd:dma_free_coherent(t->dev,PAGE_SIZE,t->bd,t->bd_dma);
free_back:dma_free_coherent(t->dev,CHUNK,t->back,t->back_dma);
free_stage:dma_free_coherent(t->dev,CHUNK,t->stage,t->stage_dma);return ret;
}
static void remove_tx(struct platform_device *pdev){
 struct ztx *t=platform_get_drvdata(pdev);int ret;
 if(clock_only)return;
 misc_deregister(&t->misc);cancel_delayed_work_sync(&t->monitor);
 mutex_lock(&t->lock);ret=stop(t);if(!ret&&t->dma_active)ret=reset_ip(t,t->copy);mutex_unlock(&t->lock);
 if(ret||t->pinned){pin(t);return;}
 kfree(t->hash);crypto_free_shash(t->sha);dma_free_coherent(t->dev,PAGE_SIZE,t->bd,t->bd_dma);dma_free_coherent(t->dev,CHUNK,t->back,t->back_dma);dma_free_coherent(t->dev,CHUNK,t->stage,t->stage_dma);
}
static const struct of_device_id match[]={ {.compatible="zcu111,vector-tx-v1"},{} };MODULE_DEVICE_TABLE(of,match);
static struct platform_driver driver={.probe=probe,.remove=remove_tx,.driver={.name="zcu111-vector-tx",.of_match_table=match,.suppress_bind_attrs=true}};
static int __init init_tx(void){
 int ret=platform_driver_register(&driver);if(ret||!manual_bind)return ret;
 {struct resource r[]={
  DEFINE_RES_MEM_NAMED(0xb0080000,0x10000,"upload"),DEFINE_RES_MEM_NAMED(0xb0090000,0x10000,"playback"),
  DEFINE_RES_MEM_NAMED(0xb00a0000,0x10000,"control"),DEFINE_RES_MEM_NAMED(0xb0040000,0x40000,"rfdc")};
  manual_device=platform_device_alloc("zcu111-vector-tx",-1);
  if(!manual_device){platform_driver_unregister(&driver);return -ENOMEM;}
  manual_device->dev.dma_mask=&manual_dma_mask;
  manual_device->dev.coherent_dma_mask=manual_dma_mask;
  ret=platform_device_add_resources(manual_device,r,clock_only?0:ARRAY_SIZE(r));
  if(!ret)ret=platform_device_add(manual_device);
  if(ret){platform_device_put(manual_device);manual_device=NULL;platform_driver_unregister(&driver);return ret;}
 }
 if(!platform_get_drvdata(manual_device)){platform_device_unregister(manual_device);manual_device=NULL;platform_driver_unregister(&driver);return -ENODEV;}
 return 0;
}
static void __exit exit_tx(void){if(manual_device)platform_device_unregister(manual_device);platform_driver_unregister(&driver);}
module_init(init_tx);module_exit(exit_tx);MODULE_LICENSE("GPL");MODULE_DESCRIPTION("ZCU111 frozen-XSA verified PL DDR cyclic TX owner");
