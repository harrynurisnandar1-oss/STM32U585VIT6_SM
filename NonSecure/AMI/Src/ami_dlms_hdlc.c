#include "ami_dlms_hdlc.h"
#include <string.h>
static size_t addr_len(uint32_t a){if(a<=0x7Fu)return 1u;if(a<=0x3FFFu)return 2u;if(a<=0x1FFFFFu)return 3u;if(a<=0x0FFFFFFFu)return 4u;return 0u;}
static size_t addr_encode(uint32_t a,uint8_t *o){size_t n=addr_len(a);if(!n)return 0u;for(size_t i=0;i<n;i++){size_t s=(n-i-1u)*7u;o[i]=(uint8_t)(((a>>s)&0x7Fu)<<1);}o[n-1u]|=1u;return n;}
static int addr_decode(const uint8_t *p,size_t n,uint32_t *a,size_t *used){uint32_t v=0u;if(!p||!a||!used)return 0;for(size_t i=0;i<n&&i<AMI_HDLC_MAX_ADDRESS_LENGTH;i++){if(v>(UINT32_MAX>>7))return 0;v=(v<<7)|(uint32_t)(p[i]>>1);if(p[i]&1u){*a=v;*used=i+1u;return 1;}}return 0;}
uint16_t ami_dlms_hdlc_crc16(const uint8_t *p,size_t n){uint16_t c=0xFFFFu;if(!p&&n)return 0u;for(size_t i=0;i<n;i++){c^=p[i];for(unsigned b=0;b<8;b++)c=(c&1u)?(uint16_t)((c>>1)^0x8408u):(uint16_t)(c>>1);}return (uint16_t)~c;}
static void put_crc(uint8_t *p,uint16_t c){p[0]=(uint8_t)c;p[1]=(uint8_t)(c>>8);}
static uint16_t get_crc(const uint8_t *p){return (uint16_t)p[0]|(uint16_t)((uint16_t)p[1]<<8);}
void ami_dlms_hdlc_init(HdlcContext *c){if(c)memset(c,0,sizeof(*c));}
AmiHdlcStatus ami_dlms_hdlc_encode_frame(uint32_t dst,uint32_t src,uint8_t control,const uint8_t *payload,size_t plen,uint8_t *out,size_t cap,size_t *olen){
 uint8_t ad[2u*AMI_HDLC_MAX_ADDRESS_LENGTH];size_t dn,sn,bl,off=3u;
 if(!out||!olen||(!payload&&plen))return AMI_HDLC_INVALID_ARGUMENT;*olen=0u;if(plen>AMI_HDLC_MAX_INFO_LENGTH)return AMI_HDLC_INFO_TOO_LARGE;
 dn=addr_encode(dst,ad);if(!dn)return AMI_HDLC_INVALID_ADDRESS;sn=addr_encode(src,ad+dn);if(!sn)return AMI_HDLC_INVALID_ADDRESS;
 bl=dn+sn+1u+(plen?plen+2u:0u)+2u;if(bl>0x7FFu)return AMI_HDLC_INFO_TOO_LARGE;if(cap<bl+4u)return AMI_HDLC_BUFFER_TOO_SMALL;
 out[0]=AMI_HDLC_FLAG;out[1]=(uint8_t)(0xA0u|((bl>>8)&7u));out[2]=(uint8_t)bl;memcpy(out+off,ad,dn+sn);off+=dn+sn;out[off++]=control;
 if(plen){put_crc(out+off,ami_dlms_hdlc_crc16(out+3u,off-3u));off+=2u;memcpy(out+off,payload,plen);off+=plen;}
 put_crc(out+off,ami_dlms_hdlc_crc16(out+3u,off-3u));off+=2u;out[off++]=AMI_HDLC_FLAG;*olen=off;return AMI_HDLC_FRAME_READY;
}
static AmiHdlcStatus decode_raw(const uint8_t *raw,size_t n,HdlcFrame *f){
 size_t off=2u,used,remain,decl;uint32_t dst,src;if(n<7u||(raw[0]&0xF0u)!=0xA0u)return AMI_HDLC_INVALID_FRAME;
 decl=((size_t)(raw[0]&7u)<<8)|raw[1];if(decl!=n-2u)return AMI_HDLC_INVALID_FRAME;
 if(!addr_decode(raw+off,n-off,&dst,&used))return AMI_HDLC_INVALID_ADDRESS;off+=used;
 if(!addr_decode(raw+off,n-off,&src,&used))return AMI_HDLC_INVALID_ADDRESS;off+=used;
 if(off>=n-2u)return AMI_HDLC_INVALID_FRAME;f->control=raw[off++];remain=n-off;if(remain<2u)return AMI_HDLC_INVALID_FRAME;
 if(remain>2u){if(remain<5u)return AMI_HDLC_INVALID_FRAME;if(get_crc(raw+off)!=ami_dlms_hdlc_crc16(raw+2u,off-2u))return AMI_HDLC_HCS_ERROR;off+=2u;
 f->payload_length=n-off-2u;if(f->payload_length>sizeof(f->payload))return AMI_HDLC_INFO_TOO_LARGE;memcpy(f->payload,raw+off,f->payload_length);off+=f->payload_length;}
 if(get_crc(raw+off)!=ami_dlms_hdlc_crc16(raw+2u,off-2u))return AMI_HDLC_FCS_ERROR;
 f->destination_sap=dst;f->source_sap=src;if(f->control==AMI_HDLC_CONTROL_SNRM)f->type=AMI_HDLC_FRAME_SNRM;
 else if(f->control==AMI_HDLC_CONTROL_UA)f->type=AMI_HDLC_FRAME_UA;else if(f->control==AMI_HDLC_CONTROL_DISC)f->type=AMI_HDLC_FRAME_DISC;
 else if((f->control&1u)==0u)f->type=AMI_HDLC_FRAME_I;else f->type=AMI_HDLC_FRAME_OTHER_CONTROL;return AMI_HDLC_FRAME_READY;
}
AmiHdlcStatus ami_dlms_hdlc_parse_byte(HdlcContext *c,uint8_t byte,HdlcFrame *f){
 AmiHdlcStatus s;
 if(!c||!f)return AMI_HDLC_INVALID_ARGUMENT;
 if(byte==AMI_HDLC_FLAG&&!c->collecting){c->collecting=1u;c->frame_length=0u;c->overflow=0u;return AMI_HDLC_INCOMPLETE;}
 if(!c->collecting)return AMI_HDLC_INCOMPLETE;
 if(c->overflow){
  if(byte==AMI_HDLC_FLAG){c->overflow=0u;c->frame_length=0u;}
  return byte==AMI_HDLC_FLAG?AMI_HDLC_BUFFER_OVERFLOW:AMI_HDLC_INCOMPLETE;
 }
 if(byte==AMI_HDLC_FLAG&&c->frame_length==0u)return AMI_HDLC_INCOMPLETE;
 if(byte==AMI_HDLC_FLAG&&c->frame_length>=2u){
  size_t expected=2u+(((size_t)(c->frame_buffer[0]&7u)<<8)|c->frame_buffer[1]);
  if(c->frame_length==expected){
   memset(f,0,sizeof(*f));s=decode_raw(c->frame_buffer,c->frame_length,f);c->frame_length=0u;
   if(s==AMI_HDLC_FRAME_READY&&f->type==AMI_HDLC_FRAME_I)c->rx_sequence=(uint8_t)((((f->control>>1)&7u)+1u)&7u);
   return s;
  }
   c->frame_length=0u;
   return AMI_HDLC_INVALID_FRAME;
 }
  if(byte==AMI_HDLC_FLAG){c->frame_length=0u;return AMI_HDLC_INVALID_FRAME;}
 if(c->frame_length>=sizeof(c->frame_buffer)){c->overflow=1u;return AMI_HDLC_INCOMPLETE;}
 c->frame_buffer[c->frame_length++]=byte;
 return AMI_HDLC_INCOMPLETE;
}AmiHdlcStatus ami_dlms_hdlc_build_frame(HdlcContext *c,const HdlcFrame *r,const uint8_t *p,size_t n,uint8_t *out,size_t cap,size_t *olen){
 uint8_t control;AmiHdlcStatus s;if(!c||!r||!out||!olen||(!p&&n))return AMI_HDLC_INVALID_ARGUMENT;
 if(r->type==AMI_HDLC_FRAME_SNRM||r->type==AMI_HDLC_FRAME_DISC){if(n)return AMI_HDLC_INVALID_ARGUMENT;s=ami_dlms_hdlc_encode_frame(r->source_sap,r->destination_sap,AMI_HDLC_CONTROL_UA,NULL,0u,out,cap,olen);if(s==AMI_HDLC_FRAME_READY){c->tx_sequence=0u;c->rx_sequence=0u;}return s;}
 if(r->type!=AMI_HDLC_FRAME_I)return AMI_HDLC_UNSUPPORTED_CONTROL;
 control=(uint8_t)(((c->tx_sequence&7u)<<1)|((c->rx_sequence&7u)<<5)|(r->control&0x10u));
 s=ami_dlms_hdlc_encode_frame(r->source_sap,r->destination_sap,control,p,n,out,cap,olen);if(s==AMI_HDLC_FRAME_READY)c->tx_sequence=(uint8_t)((c->tx_sequence+1u)&7u);return s;
}

