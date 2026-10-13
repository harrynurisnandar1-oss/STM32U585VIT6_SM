#include "ami_dlms_hdlc.h"
#include <assert.h>
#include <string.h>
static AmiHdlcStatus feed(HdlcContext *c,const uint8_t *w,size_t n,HdlcFrame *f){AmiHdlcStatus s=AMI_HDLC_INCOMPLETE;for(size_t i=0;i<n;i++)s=ami_dlms_hdlc_parse_byte(c,w[i],f);return s;}
static void test_crc_and_stream_roundtrip(void){static const uint8_t v[]="123456789";const uint8_t p[]={0xC0,1,0x55,0};uint8_t w[64];size_t n=0;HdlcContext c;HdlcFrame f;
 assert(ami_dlms_hdlc_crc16(v,sizeof(v)-1u)==0x906Eu);assert(ami_dlms_hdlc_encode_frame(1,16,0x10,p,sizeof(p),w,sizeof(w),&n)==AMI_HDLC_FRAME_READY);
 ami_dlms_hdlc_init(&c);assert(feed(&c,w,n,&f)==AMI_HDLC_FRAME_READY);assert(f.destination_sap==1u&&f.source_sap==16u&&f.type==AMI_HDLC_FRAME_I);assert(f.payload_length==sizeof(p)&&memcmp(f.payload,p,sizeof(p))==0);}
static void test_sap_and_output_boundaries(void){uint8_t w[64];size_t n;HdlcContext c;HdlcFrame f;
 assert(ami_dlms_hdlc_encode_frame(0x0FFFFFFF,0x3FFF,0x10,NULL,0,w,sizeof(w),&n)==AMI_HDLC_FRAME_READY);ami_dlms_hdlc_init(&c);assert(feed(&c,w,n,&f)==AMI_HDLC_FRAME_READY);
 assert(f.destination_sap==0x0FFFFFFFu&&f.source_sap==0x3FFFu);assert(ami_dlms_hdlc_encode_frame(0x10000000,1,0x10,NULL,0,w,sizeof(w),&n)==AMI_HDLC_INVALID_ADDRESS);
 assert(ami_dlms_hdlc_encode_frame(1,1,0x10,NULL,0,w,2,&n)==AMI_HDLC_BUFFER_TOO_SMALL);}
static void test_crc_and_truncated_errors(void){const uint8_t p[]={0x55};uint8_t w[64];size_t n;HdlcContext c;HdlcFrame f;
 assert(ami_dlms_hdlc_encode_frame(1,16,0x10,p,sizeof(p),w,sizeof(w),&n)==AMI_HDLC_FRAME_READY);w[n-3u]^=1u;ami_dlms_hdlc_init(&c);assert(feed(&c,w,n,&f)==AMI_HDLC_FCS_ERROR);
 assert(ami_dlms_hdlc_encode_frame(1,16,0x10,p,sizeof(p),w,sizeof(w),&n)==AMI_HDLC_FRAME_READY);w[6]^=1u;ami_dlms_hdlc_init(&c);assert(feed(&c,w,n,&f)==AMI_HDLC_HCS_ERROR);
 assert(ami_dlms_hdlc_encode_frame(1,16,0x10,p,sizeof(p),w,sizeof(w),&n)==AMI_HDLC_FRAME_READY);ami_dlms_hdlc_init(&c);assert(feed(&c,w,n-1u,&f)==AMI_HDLC_INCOMPLETE);}
static void test_snrm_disc_to_ua_and_i_reply(void){
 HdlcContext c;HdlcFrame r={.destination_sap=1,.source_sap=16,.control=AMI_HDLC_CONTROL_SNRM,.type=AMI_HDLC_FRAME_SNRM},f;uint8_t w[64];size_t n;ami_dlms_hdlc_init(&c);
  c.tx_sequence=5u;c.rx_sequence=4u;assert(ami_dlms_hdlc_build_frame(&c,&r,NULL,0,w,sizeof(w),&n)==AMI_HDLC_FRAME_READY);assert(c.tx_sequence==0u&&c.rx_sequence==0u);ami_dlms_hdlc_init(&c);assert(feed(&c,w,n,&f)==AMI_HDLC_FRAME_READY);assert(f.type==AMI_HDLC_FRAME_UA&&f.destination_sap==16u&&f.source_sap==1u);
 r.type=AMI_HDLC_FRAME_DISC;r.control=AMI_HDLC_CONTROL_DISC;
 assert(ami_dlms_hdlc_build_frame(&c,&r,NULL,0,w,sizeof(w),&n)==AMI_HDLC_FRAME_READY);ami_dlms_hdlc_init(&c);assert(feed(&c,w,n,&f)==AMI_HDLC_FRAME_READY);assert(f.type==AMI_HDLC_FRAME_UA);
 r.type=AMI_HDLC_FRAME_I;r.control=0x10;const uint8_t p[]={0xC4,1,0};assert(ami_dlms_hdlc_build_frame(&c,&r,p,sizeof(p),w,sizeof(w),&n)==AMI_HDLC_FRAME_READY);assert(feed(&c,w,n,&f)==AMI_HDLC_FRAME_READY&&f.type==AMI_HDLC_FRAME_I);assert(f.payload_length==sizeof(p)&&memcmp(f.payload,p,sizeof(p))==0);
 }
static void test_early_flag_resynchronizes(void){
 HdlcContext c;HdlcFrame f;uint8_t frame[64];size_t n=0;ami_dlms_hdlc_init(&c);
 assert(ami_dlms_hdlc_parse_byte(&c,AMI_HDLC_FLAG,&f)==AMI_HDLC_INCOMPLETE);
 assert(ami_dlms_hdlc_parse_byte(&c,0xA2u,&f)==AMI_HDLC_INCOMPLETE);
 assert(ami_dlms_hdlc_parse_byte(&c,0x20u,&f)==AMI_HDLC_INCOMPLETE);
 assert(ami_dlms_hdlc_parse_byte(&c,AMI_HDLC_FLAG,&f)==AMI_HDLC_INVALID_FRAME);
 assert(ami_dlms_hdlc_encode_frame(1u,16u,0x10u,NULL,0u,frame,sizeof(frame),&n)==AMI_HDLC_FRAME_READY);
 assert(feed(&c,frame,n,&f)==AMI_HDLC_FRAME_READY);
}
int main(void){test_crc_and_stream_roundtrip();test_sap_and_output_boundaries();test_crc_and_truncated_errors();test_snrm_disc_to_ua_and_i_reply();test_early_flag_resynchronizes();return 0;}

