#include <stdio.h>
#include <stdlib.h>
#define PW 720
#define PH 1280
/* the same expressions as board_bsp.c; mode 0 = A, 1 = B */
static void blit(int mode,int x,int y,int w,int h,int *dst,int *px0o,int *py0o,int *pwo){
  int i,j,px0,py0,pw;
  if(!mode){ px0=PW-(y+h); py0=x; pw=h; for(j=0;j<h;j++)for(i=0;i<w;i++) dst[i*pw+(h-1-j)]=j*w+i; }
  else     { px0=y; py0=PH-(x+w); pw=h; for(j=0;j<h;j++)for(i=0;i<w;i++) dst[(w-1-i)*pw+j]=j*w+i; }
  *px0o=px0;*py0o=py0;*pwo=pw; }
static void touch(int mode,int px,int py,int*lx,int*ly){ if(!mode){*lx=py;*ly=PW-1-px;} else {*lx=PH-1-py;*ly=px;} }
int main(){ int bad=0; int rects[4][4]={{8,8,480,480},{0,0,1280,720},{16,508,1248,200},{1000,300,37,91}};
 for(int mode=0;mode<2;mode++) for(int r=0;r<4;r++){
  int x=rects[r][0],y=rects[r][1],w=rects[r][2],h=rects[r][3]; int *dst=malloc(sizeof(int)*w*h); int px0,py0,pw;
  blit(mode,x,y,w,h,dst,&px0,&py0,&pw);
  int ph=w; if(px0<0||py0<0||px0+pw>PW||py0+ph>PH){printf("mode %d rect %d OUT OF PANEL (%d,%d %dx%d)\n",mode,r,px0,py0,pw,ph);bad++;}
  for(int idx=0;idx<w*h;idx++){ int ry=idx/pw, rx=idx%pw; int src=dst[idx]; int i=src%w,j=src/w; int px=px0+rx,py=py0+ry; int lx,ly; touch(mode,px,py,&lx,&ly);
    if(lx!=x+i||ly!=y+j){ if(bad<5)printf("mode %d rect %d MISMATCH src(%d,%d) -> panel(%d,%d) -> touch(%d,%d)\n",mode,r,x+i,y+j,px,py,lx,ly); bad++; break; } }
  free(dst); }
 printf(bad?"FAILED %d\n":"rotation + touch inverse: all rects, both modes OK\n",bad); return bad!=0; }
