#ifndef VP9_RTCD_H_
#define VP9_RTCD_H_

#ifdef RTCD_C
#define RTCD_EXTERN
#else
#define RTCD_EXTERN extern
#endif

/*
 * VP9
 */

#include "vpx/vpx_integer.h"

struct loop_filter_info;
struct blockd;
struct macroblockd;
struct loop_filter_info;

/* Encoder forward decls */
struct block;
struct macroblock;
struct vp9_variance_vtable;

#define DEC_MVCOSTS int *mvjcost, int *mvcost[2]
union int_mv;
struct yv12_buffer_config;

void vp9_dequant_idct_add_y_block_8x8_c(int16_t *q, const int16_t *dq, uint8_t *pre, uint8_t *dst, int stride, struct macroblockd *xd);
#define vp9_dequant_idct_add_y_block_8x8 vp9_dequant_idct_add_y_block_8x8_c

void vp9_dequant_idct_add_uv_block_8x8_c(int16_t *q, const int16_t *dq, uint8_t *pre, uint8_t *dstu, uint8_t *dstv, int stride, struct macroblockd *xd);
#define vp9_dequant_idct_add_uv_block_8x8 vp9_dequant_idct_add_uv_block_8x8_c

void vp9_dequant_idct_add_16x16_c(int16_t *input, const int16_t *dq, uint8_t *pred, uint8_t *dest, int pitch, int stride, int eob);
#define vp9_dequant_idct_add_16x16 vp9_dequant_idct_add_16x16_c

void vp9_dequant_idct_add_8x8_c(int16_t *input, const int16_t *dq, uint8_t *pred, uint8_t *dest, int pitch, int stride, int eob);
#define vp9_dequant_idct_add_8x8 vp9_dequant_idct_add_8x8_c

void vp9_dequant_idct_add_c(int16_t *input, const int16_t *dq, uint8_t *pred, uint8_t *dest, int pitch, int stride, int eob);
#define vp9_dequant_idct_add vp9_dequant_idct_add_c

void vp9_dequant_idct_add_y_block_c(int16_t *q, const int16_t *dq, uint8_t *pre, uint8_t *dst, int stride, struct macroblockd *xd);
#define vp9_dequant_idct_add_y_block vp9_dequant_idct_add_y_block_c

void vp9_dequant_idct_add_uv_block_c(int16_t *q, const int16_t *dq, uint8_t *pre, uint8_t *dstu, uint8_t *dstv, int stride, struct macroblockd *xd);
#define vp9_dequant_idct_add_uv_block vp9_dequant_idct_add_uv_block_c

void vp9_dequant_idct_add_32x32_c(int16_t *q, const int16_t *dq, uint8_t *pre, uint8_t *dst, int pitch, int stride, int eob);
#define vp9_dequant_idct_add_32x32 vp9_dequant_idct_add_32x32_c

void vp9_dequant_idct_add_uv_block_16x16_c(int16_t *q, const int16_t *dq, uint8_t *dstu, uint8_t *dstv, int stride, struct macroblockd *xd);
#define vp9_dequant_idct_add_uv_block_16x16 vp9_dequant_idct_add_uv_block_16x16_c

void vp9_copy_mem16x16_c(const uint8_t *src, int src_pitch, uint8_t *dst, int dst_pitch);
#define vp9_copy_mem16x16 vp9_copy_mem16x16_c

void vp9_copy_mem8x8_c(const uint8_t *src, int src_pitch, uint8_t *dst, int dst_pitch);
#define vp9_copy_mem8x8 vp9_copy_mem8x8_c

void vp9_copy_mem8x4_c(const uint8_t *src, int src_pitch, uint8_t *dst, int dst_pitch);
#define vp9_copy_mem8x4 vp9_copy_mem8x4_c

void vp9_recon_b_c(uint8_t *pred_ptr, int16_t *diff_ptr, uint8_t *dst_ptr, int stride);
#define vp9_recon_b vp9_recon_b_c

void vp9_recon_uv_b_c(uint8_t *pred_ptr, int16_t *diff_ptr, uint8_t *dst_ptr, int stride);
#define vp9_recon_uv_b vp9_recon_uv_b_c

void vp9_recon2b_c(uint8_t *pred_ptr, int16_t *diff_ptr, uint8_t *dst_ptr, int stride);
#define vp9_recon2b vp9_recon2b_c

void vp9_recon4b_c(uint8_t *pred_ptr, int16_t *diff_ptr, uint8_t *dst_ptr, int stride);
#define vp9_recon4b vp9_recon4b_c

void vp9_recon_mb_c(struct macroblockd *x);
#define vp9_recon_mb vp9_recon_mb_c

void vp9_recon_mby_c(struct macroblockd *x);
#define vp9_recon_mby vp9_recon_mby_c

void vp9_recon_mby_s_c(struct macroblockd *x, uint8_t *dst);
#define vp9_recon_mby_s vp9_recon_mby_s_c

void vp9_recon_mbuv_s_c(struct macroblockd *x, uint8_t *udst, uint8_t *vdst);
#define vp9_recon_mbuv_s vp9_recon_mbuv_s_c

void vp9_recon_sby_s_c(struct macroblockd *x, uint8_t *dst);
#define vp9_recon_sby_s vp9_recon_sby_s_c

void vp9_recon_sbuv_s_c(struct macroblockd *x, uint8_t *udst, uint8_t *vdst);
#define vp9_recon_sbuv_s vp9_recon_sbuv_s_c

void vp9_recon_sb64y_s_c(struct macroblockd *x, uint8_t *dst);
#define vp9_recon_sb64y_s vp9_recon_sb64y_s_c

void vp9_recon_sb64uv_s_c(struct macroblockd *x, uint8_t *udst, uint8_t *vdst);
#define vp9_recon_sb64uv_s vp9_recon_sb64uv_s_c

void vp9_build_intra_predictors_mby_s_c(struct macroblockd *x);
#define vp9_build_intra_predictors_mby_s vp9_build_intra_predictors_mby_s_c

void vp9_build_intra_predictors_sby_s_c(struct macroblockd *x);
#define vp9_build_intra_predictors_sby_s vp9_build_intra_predictors_sby_s_c

void vp9_build_intra_predictors_sbuv_s_c(struct macroblockd *x);
#define vp9_build_intra_predictors_sbuv_s vp9_build_intra_predictors_sbuv_s_c

void vp9_build_intra_predictors_mby_c(struct macroblockd *x);
#define vp9_build_intra_predictors_mby vp9_build_intra_predictors_mby_c

void vp9_build_intra_predictors_mby_s_c(struct macroblockd *x);
#define vp9_build_intra_predictors_mby_s vp9_build_intra_predictors_mby_s_c

void vp9_build_intra_predictors_mbuv_c(struct macroblockd *x);
#define vp9_build_intra_predictors_mbuv vp9_build_intra_predictors_mbuv_c

void vp9_build_intra_predictors_mbuv_s_c(struct macroblockd *x);
#define vp9_build_intra_predictors_mbuv_s vp9_build_intra_predictors_mbuv_s_c

void vp9_build_intra_predictors_sb64y_s_c(struct macroblockd *x);
#define vp9_build_intra_predictors_sb64y_s vp9_build_intra_predictors_sb64y_s_c

void vp9_build_intra_predictors_sb64uv_s_c(struct macroblockd *x);
#define vp9_build_intra_predictors_sb64uv_s vp9_build_intra_predictors_sb64uv_s_c

void vp9_intra4x4_predict_c(struct macroblockd *xd, struct blockd *x, int b_mode, uint8_t *predictor);
#define vp9_intra4x4_predict vp9_intra4x4_predict_c

void vp9_intra8x8_predict_c(struct macroblockd *xd, struct blockd *x, int b_mode, uint8_t *predictor);
#define vp9_intra8x8_predict vp9_intra8x8_predict_c

void vp9_intra_uv4x4_predict_c(struct macroblockd *xd, struct blockd *x, int b_mode, uint8_t *predictor);
#define vp9_intra_uv4x4_predict vp9_intra_uv4x4_predict_c

void vp9_add_residual_4x4_c(const int16_t *diff, const uint8_t *pred, int pitch, uint8_t *dest, int stride);
#define vp9_add_residual_4x4 vp9_add_residual_4x4_c

void vp9_add_residual_8x8_c(const int16_t *diff, const uint8_t *pred, int pitch, uint8_t *dest, int stride);
#define vp9_add_residual_8x8 vp9_add_residual_8x8_c

void vp9_add_residual_16x16_c(const int16_t *diff, const uint8_t *pred, int pitch, uint8_t *dest, int stride);
#define vp9_add_residual_16x16 vp9_add_residual_16x16_c

void vp9_add_residual_32x32_c(const int16_t *diff, const uint8_t *pred, int pitch, uint8_t *dest, int stride);
#define vp9_add_residual_32x32 vp9_add_residual_32x32_c

void vp9_add_constant_residual_8x8_c(const int16_t diff, const uint8_t *pred, int pitch, uint8_t *dest, int stride);
#define vp9_add_constant_residual_8x8 vp9_add_constant_residual_8x8_c

void vp9_add_constant_residual_16x16_c(const int16_t diff, const uint8_t *pred, int pitch, uint8_t *dest, int stride);
#define vp9_add_constant_residual_16x16 vp9_add_constant_residual_16x16_c

void vp9_add_constant_residual_32x32_c(const int16_t diff, const uint8_t *pred, int pitch, uint8_t *dest, int stride);
#define vp9_add_constant_residual_32x32 vp9_add_constant_residual_32x32_c

void vp9_loop_filter_mbv_c(uint8_t *y, uint8_t *u, uint8_t *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
#define vp9_loop_filter_mbv vp9_loop_filter_mbv_c

void vp9_loop_filter_bv_c(uint8_t *y, uint8_t *u, uint8_t *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
#define vp9_loop_filter_bv vp9_loop_filter_bv_c

void vp9_loop_filter_bv8x8_c(uint8_t *y, uint8_t *u, uint8_t *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
#define vp9_loop_filter_bv8x8 vp9_loop_filter_bv8x8_c

void vp9_loop_filter_mbh_c(uint8_t *y, uint8_t *u, uint8_t *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
#define vp9_loop_filter_mbh vp9_loop_filter_mbh_c

void vp9_loop_filter_bh_c(uint8_t *y, uint8_t *u, uint8_t *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
#define vp9_loop_filter_bh vp9_loop_filter_bh_c

void vp9_loop_filter_bh8x8_c(uint8_t *y, uint8_t *u, uint8_t *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
#define vp9_loop_filter_bh8x8 vp9_loop_filter_bh8x8_c

void vp9_loop_filter_simple_vertical_edge_c(uint8_t *y, int ystride, const uint8_t *blimit);
#define vp9_loop_filter_simple_mbv vp9_loop_filter_simple_vertical_edge_c

void vp9_loop_filter_simple_horizontal_edge_c(uint8_t *y, int ystride, const uint8_t *blimit);
#define vp9_loop_filter_simple_mbh vp9_loop_filter_simple_horizontal_edge_c

void vp9_loop_filter_bvs_c(uint8_t *y, int ystride, const uint8_t *blimit);
#define vp9_loop_filter_simple_bv vp9_loop_filter_bvs_c

void vp9_loop_filter_bhs_c(uint8_t *y, int ystride, const uint8_t *blimit);
#define vp9_loop_filter_simple_bh vp9_loop_filter_bhs_c

void vp9_lpf_mbh_w_c(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr, int y_stride, int uv_stride, struct loop_filter_info *lfi);
#define vp9_lpf_mbh_w vp9_lpf_mbh_w_c

void vp9_lpf_mbv_w_c(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr, int y_stride, int uv_stride, struct loop_filter_info *lfi);
#define vp9_lpf_mbv_w vp9_lpf_mbv_w_c

void vp9_mbpost_proc_down_c(uint8_t *dst, int pitch, int rows, int cols, int flimit);
#define vp9_mbpost_proc_down vp9_mbpost_proc_down_c

void vp9_mbpost_proc_across_ip_c(uint8_t *src, int pitch, int rows, int cols, int flimit);
#define vp9_mbpost_proc_across_ip vp9_mbpost_proc_across_ip_c

void vp9_post_proc_down_and_across_c(uint8_t *src_ptr, uint8_t *dst_ptr, int src_pixels_per_line, int dst_pixels_per_line, int rows, int cols, int flimit);
#define vp9_post_proc_down_and_across vp9_post_proc_down_and_across_c

void vp9_plane_add_noise_c(uint8_t *Start, char *noise, char blackclamp[16], char whiteclamp[16], char bothclamp[16], unsigned int Width, unsigned int Height, int Pitch);
#define vp9_plane_add_noise vp9_plane_add_noise_c

void vp9_blend_mb_inner_c(uint8_t *y, uint8_t *u, uint8_t *v, int y1, int u1, int v1, int alpha, int stride);
#define vp9_blend_mb_inner vp9_blend_mb_inner_c

void vp9_blend_mb_outer_c(uint8_t *y, uint8_t *u, uint8_t *v, int y1, int u1, int v1, int alpha, int stride);
#define vp9_blend_mb_outer vp9_blend_mb_outer_c

void vp9_blend_b_c(uint8_t *y, uint8_t *u, uint8_t *v, int y1, int u1, int v1, int alpha, int stride);
#define vp9_blend_b vp9_blend_b_c

unsigned int vp9_sad16x3_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int ref_stride);
#define vp9_sad16x3 vp9_sad16x3_c

unsigned int vp9_sad3x16_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int ref_stride);
#define vp9_sad3x16 vp9_sad3x16_c

unsigned int vp9_sub_pixel_variance16x2_c(const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vp9_sub_pixel_variance16x2 vp9_sub_pixel_variance16x2_c

void vp9_convolve8_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8 vp9_convolve8_c

void vp9_convolve8_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_horiz vp9_convolve8_horiz_c

void vp9_convolve8_vert_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_vert vp9_convolve8_vert_c

void vp9_convolve8_avg_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_avg vp9_convolve8_avg_c

void vp9_convolve8_avg_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_avg_horiz vp9_convolve8_avg_horiz_c

void vp9_convolve8_avg_vert_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_avg_vert vp9_convolve8_avg_vert_c

void vp9_convolve8_1by8_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_1by8 vp9_convolve8_1by8_c

void vp9_convolve8_qtr_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_qtr vp9_convolve8_qtr_c

void vp9_convolve8_3by8_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_3by8 vp9_convolve8_3by8_c

void vp9_convolve8_5by8_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_5by8 vp9_convolve8_5by8_c

void vp9_convolve8_3qtr_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_3qtr vp9_convolve8_3qtr_c

void vp9_convolve8_7by8_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_7by8 vp9_convolve8_7by8_c

void vp9_convolve8_1by8_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_1by8_horiz vp9_convolve8_1by8_horiz_c

void vp9_convolve8_qtr_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_qtr_horiz vp9_convolve8_qtr_horiz_c

void vp9_convolve8_3by8_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_3by8_horiz vp9_convolve8_3by8_horiz_c

void vp9_convolve8_5by8_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_5by8_horiz vp9_convolve8_5by8_horiz_c

void vp9_convolve8_3qtr_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_3qtr_horiz vp9_convolve8_3qtr_horiz_c

void vp9_convolve8_7by8_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_7by8_horiz vp9_convolve8_7by8_horiz_c

void vp9_convolve8_1by8_vert_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_1by8_vert vp9_convolve8_1by8_vert_c

void vp9_convolve8_qtr_vert_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_qtr_vert vp9_convolve8_qtr_vert_c

void vp9_convolve8_3by8_vert_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_3by8_vert vp9_convolve8_3by8_vert_c

void vp9_convolve8_5by8_vert_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_5by8_vert vp9_convolve8_5by8_vert_c

void vp9_convolve8_3qtr_vert_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_3qtr_vert vp9_convolve8_3qtr_vert_c

void vp9_convolve8_7by8_vert_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_7by8_vert vp9_convolve8_7by8_vert_c

void vp9_short_idct4x4_1_c(int16_t *input, int16_t *output, int pitch);
#define vp9_short_idct4x4_1 vp9_short_idct4x4_1_c

void vp9_short_idct4x4_c(int16_t *input, int16_t *output, int pitch);
#define vp9_short_idct4x4 vp9_short_idct4x4_c

void vp9_short_idct8x8_c(int16_t *input, int16_t *output, int pitch);
#define vp9_short_idct8x8 vp9_short_idct8x8_c

void vp9_short_idct10_8x8_c(int16_t *input, int16_t *output, int pitch);
#define vp9_short_idct10_8x8 vp9_short_idct10_8x8_c

void vp9_short_idct1_8x8_c(int16_t *input, int16_t *output);
#define vp9_short_idct1_8x8 vp9_short_idct1_8x8_c

void vp9_short_idct16x16_c(int16_t *input, int16_t *output, int pitch);
#define vp9_short_idct16x16 vp9_short_idct16x16_c

void vp9_short_idct10_16x16_c(int16_t *input, int16_t *output, int pitch);
#define vp9_short_idct10_16x16 vp9_short_idct10_16x16_c

void vp9_short_idct1_16x16_c(int16_t *input, int16_t *output);
#define vp9_short_idct1_16x16 vp9_short_idct1_16x16_c

void vp9_short_idct32x32_c(int16_t *input, int16_t *output, int pitch);
#define vp9_short_idct32x32 vp9_short_idct32x32_c

void vp9_short_idct1_32x32_c(int16_t *input, int16_t *output);
#define vp9_short_idct1_32x32 vp9_short_idct1_32x32_c

void vp9_short_idct10_32x32_c(int16_t *input, int16_t *output, int pitch);
#define vp9_short_idct10_32x32 vp9_short_idct10_32x32_c

void vp9_short_iht8x8_c(int16_t *input, int16_t *output, int pitch, int tx_type);
#define vp9_short_iht8x8 vp9_short_iht8x8_c

void vp9_short_iht4x4_c(int16_t *input, int16_t *output, int pitch, int tx_type);
#define vp9_short_iht4x4 vp9_short_iht4x4_c

void vp9_short_iht16x16_c(int16_t *input, int16_t *output, int pitch, int tx_type);
#define vp9_short_iht16x16 vp9_short_iht16x16_c

void vp9_idct4_1d_c(int16_t *input, int16_t *output);
#define vp9_idct4_1d vp9_idct4_1d_c

void vp9_dc_only_idct_add_c(int input_dc, uint8_t *pred_ptr, uint8_t *dst_ptr, int pitch, int stride);
#define vp9_dc_only_idct_add vp9_dc_only_idct_add_c

void vp9_short_iwalsh4x4_1_c(int16_t *input, int16_t *output, int pitch);
#define vp9_short_iwalsh4x4_1 vp9_short_iwalsh4x4_1_c

void vp9_short_iwalsh4x4_c(int16_t *input, int16_t *output, int pitch);
#define vp9_short_iwalsh4x4 vp9_short_iwalsh4x4_c

void vp9_dc_only_inv_walsh_add_c(int input_dc, uint8_t *pred_ptr, uint8_t *dst_ptr, int pitch, int stride);
#define vp9_dc_only_inv_walsh_add vp9_dc_only_inv_walsh_add_c

unsigned int vp9_sad32x3_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int ref_stride, int max_sad);
#define vp9_sad32x3 vp9_sad32x3_c

unsigned int vp9_sad3x32_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int ref_stride, int max_sad);
#define vp9_sad3x32 vp9_sad3x32_c

void vp9_rtcd(void);
#include "vpx_config.h"

#ifdef RTCD_C
static void setup_rtcd_internal(void)
{

}
#endif
#endif
