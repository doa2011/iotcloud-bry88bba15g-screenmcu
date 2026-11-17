#include "screen.h"
#include "sensors.h"
#include <string.h>
bool sensor_uart_value(const uint8_t *p,unsigned n,uint8_t cmd,uint16_t *v){
 if(!p||!v||n<6||p[0]!=0x16||n!=(unsigned)p[1]+3||p[2]!=cmd)return false;
 uint8_t sum=0;for(unsigned i=0;i<n;i++)sum+=p[i];if(sum)return false;
 *v=((uint16_t)p[3]<<8)|p[4];return true;
}
static uint8_t crc(const uint8_t *p){uint8_t c=255;for(unsigned i=0;i<2;i++){c^=p[i];for(unsigned j=0;j<8;j++)c=(c&128)?(c<<1)^0x31:c<<1;}return c;}
bool sensor_sht3x(const uint8_t p[6],float *t,float *h){
 if(!p||!t||!h||crc(p)!=p[2]||crc(p+3)!=p[5])return false;
 /* Raw conversion only. Stateful stock compensation is in sht_compensation.c. */
 *t=(float)(175.0*(((unsigned)p[0]<<8)|p[1])/65534.0-45.0);
 *h=100.0f*(((unsigned)p[3]<<8)|p[4])/65534.0f;return true;
}
unsigned battery_permille(unsigned mv){
 static const unsigned curve[]={3233,3435,3532,3585,3616,3662,3749,3842,3943,4053,4185};
 if(mv<=curve[0])return 0;
 if(mv>=curve[10])return 1000;
 for(unsigned i=1;i<11;i++)if(mv<curve[i])return (i-1)*100+(mv-curve[i-1])*100/(curve[i]-curve[i-1]);
 return 1000;
}

/* 0x4a888: CMD=1 requires frame[13], beyond the common value envelope. */
bool sensor_hcho(const uint8_t *p,unsigned n,uint16_t *value,uint8_t *status){
 uint16_t raw;
 if(!value||!status||n<14||!sensor_uart_value(p,n,1,&raw))return false;
 float x=(float)((double)raw*1.25/10.0);
 x=(float)((double)x+1.0);
 if(x>50.0f)x=50.0f;
 *value=(uint16_t)x;*status=p[13];return true;
}
unsigned sensor_poll_next(sensor_poll_state *s) {
 unsigned slot=s->slot++ % 4;
 if(slot==SENSOR_POLL_SHT_COMMAND){
  unsigned result=s->sht_read?SENSOR_POLL_SHT_READ:SENSOR_POLL_SHT_COMMAND;
  s->sht_read=!s->sht_read;return result;
 }
 return slot;
}
uint16_t sensor_pm25(uint16_t raw,uint16_t coefficient) {
 float value=raw*0.01f*coefficient;
 return value>300.0f?300:(uint16_t)value;
}
uint16_t sensor_co2(uint16_t raw) {return raw>5000?5000:raw;}
void sensor_air_init(sensor_air_state *s,uint32_t last_stop) {
 memset(s,0,sizeof(*s));s->last_stop=last_stop;s->warmup_minutes=-1;
 s->pm_status=s->tvoc_status=0x52;
}
void sensor_air_receive(sensor_air_state *s,bool measurement) {
 s->missed=0;if(measurement)s->received=1;
}
bool sensor_air_warmup(const sensor_air_state *s,uint32_t now,
 uint32_t *duration,uint32_t *elapsed,uint32_t *remaining) {
 if(!s->received||s->phase<1||s->phase>3)return false;
 *duration=s->warmup_minutes>=0?(uint32_t)s->warmup_minutes*60000u:
     (s->phase==1?3600u:s->phase==2?1800u:300u)*1000u;
 uint32_t age=now-s->started;
 *elapsed=age<*duration?age:*duration;
 *remaining=*duration-*elapsed;return true;
}
bool sensor_air_update(sensor_air_state *s,uint32_t utc,uint32_t now) {
 bool save=false;
 if(s->previous!=s->received){
  if(s->received){
   uint32_t gap=utc&&s->last_stop&&utc>=s->last_stop?utc-s->last_stop:172801;s->started=now;
   s->phase=gap>=172801?1:gap>=3600?2:3;
  }else{
   if(s->tvoc_status==0x51&&utc&&s->last_stop!=utc){s->last_stop=utc;save=true;}
   s->phase=0;
  }
 }
 if(!s->phase){s->mode=0;s->pm_status=s->tvoc_status=0x52;}
 else if(s->phase<=3){
  uint32_t duration,elapsed,remaining;
  bool warming=sensor_air_warmup(s,now,&duration,&elapsed,&remaining)&&
      remaining!=0;
  s->mode=warming?(uint8_t)(4-s->phase):0;
  s->pm_status=0x51;s->tvoc_status=warming?0x56:0x51;
 }else s->pm_status=0x52;
 s->previous=s->received;return save;
}
unsigned sensor_air_poll(sensor_air_state *s,uint32_t utc,uint32_t now,uint16_t co2,
 uint16_t hcho,uint8_t hcho_status,int32_t temperature10,int32_t humidity10,
 uint8_t frame[12],bool *save_stop) {
 unsigned n;
 /* The original sends the old mode before advancing its state. */
 if(!(s->polls++&1)){
  const uint8_t read[]={0x11,1,0x16,0xd8};memcpy(frame,read,4);n=4;
  if(s->missed<=2)s->missed++;
 }else{
  uint8_t control[]={0x11,9,0x0b,s->mode,
   co2<=1000&&(hcho_status==0x59||hcho<=9),
   (uint16_t)temperature10>>8,(uint8_t)temperature10,
   (uint16_t)humidity10>>8,(uint8_t)humidity10,0,0,0};
  uint8_t sum=0;for(unsigned i=0;i<11;i++)sum+=control[i];
  control[11]=(uint8_t)-sum;memcpy(frame,control,12);n=12;
 }
 *save_stop=sensor_air_update(s,utc,now);return n;
}
