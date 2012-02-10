# 1 "arch/powerpc/kernel/systbl_chk.c"
# 1 "<built-in>"
# 1 "<command line>"
# 1 "./include/linux/autoconf.h" 1
# 1 "<command line>" 2
# 1 "arch/powerpc/kernel/systbl_chk.c"
# 16 "arch/powerpc/kernel/systbl_chk.c"
# 1 "/home/volatile/svnROACH/sw/borph_mainline_linux/arch/powerpc/include/asm/unistd.h" 1
# 354 "/home/volatile/svnROACH/sw/borph_mainline_linux/arch/powerpc/include/asm/unistd.h"
# 1 "include/linux/types.h" 1
# 11 "include/linux/types.h"
# 1 "include/linux/posix_types.h" 1



# 1 "include/linux/stddef.h" 1



# 1 "include/linux/compiler.h" 1
# 40 "include/linux/compiler.h"
# 1 "include/linux/compiler-gcc4.h" 1





# 1 "include/linux/compiler-gcc.h" 1
# 7 "include/linux/compiler-gcc4.h" 2
# 41 "include/linux/compiler.h" 2
# 5 "include/linux/stddef.h" 2
# 15 "include/linux/stddef.h"
enum {
 false = 0,
 true = 1
};
# 5 "include/linux/posix_types.h" 2
# 36 "include/linux/posix_types.h"
typedef struct {
 unsigned long fds_bits [(1024/(8 * sizeof(unsigned long)))];
} __kernel_fd_set;


typedef void (*__kernel_sighandler_t)(int);


typedef int __kernel_key_t;
typedef int __kernel_mqd_t;

# 1 "/home/volatile/svnROACH/sw/borph_mainline_linux/arch/powerpc/include/asm/posix_types.h" 1
# 10 "/home/volatile/svnROACH/sw/borph_mainline_linux/arch/powerpc/include/asm/posix_types.h"
typedef unsigned long __kernel_ino_t;
typedef unsigned int __kernel_mode_t;
typedef long __kernel_off_t;
typedef int __kernel_pid_t;
typedef unsigned int __kernel_uid_t;
typedef unsigned int __kernel_gid_t;
typedef long __kernel_ptrdiff_t;
typedef long __kernel_time_t;
typedef long __kernel_clock_t;
typedef int __kernel_timer_t;
typedef int __kernel_clockid_t;
typedef long __kernel_suseconds_t;
typedef int __kernel_daddr_t;
typedef char * __kernel_caddr_t;
typedef unsigned short __kernel_uid16_t;
typedef unsigned short __kernel_gid16_t;
typedef unsigned int __kernel_uid32_t;
typedef unsigned int __kernel_gid32_t;
typedef unsigned int __kernel_old_uid_t;
typedef unsigned int __kernel_old_gid_t;
# 38 "/home/volatile/svnROACH/sw/borph_mainline_linux/arch/powerpc/include/asm/posix_types.h"
typedef unsigned short __kernel_nlink_t;
typedef short __kernel_ipc_pid_t;
typedef unsigned int __kernel_size_t;
typedef int __kernel_ssize_t;
typedef unsigned int __kernel_old_dev_t;






typedef long long __kernel_loff_t;



typedef struct {
 int val[2];
} __kernel_fsid_t;
# 71 "/home/volatile/svnROACH/sw/borph_mainline_linux/arch/powerpc/include/asm/posix_types.h"
static __inline__ __attribute__((always_inline)) void __FD_SET(unsigned long fd, __kernel_fd_set *fdsetp)
{
 unsigned long _tmp = fd / (8 * sizeof(unsigned long));
 unsigned long _rem = fd % (8 * sizeof(unsigned long));
 fdsetp->fds_bits[_tmp] |= (1UL<<_rem);
}


static __inline__ __attribute__((always_inline)) void __FD_CLR(unsigned long fd, __kernel_fd_set *fdsetp)
{
 unsigned long _tmp = fd / (8 * sizeof(unsigned long));
 unsigned long _rem = fd % (8 * sizeof(unsigned long));
 fdsetp->fds_bits[_tmp] &= ~(1UL<<_rem);
}


static __inline__ __attribute__((always_inline)) int __FD_ISSET(unsigned long fd, __kernel_fd_set *p)
{
 unsigned long _tmp = fd / (8 * sizeof(unsigned long));
 unsigned long _rem = fd % (8 * sizeof(unsigned long));
 return (p->fds_bits[_tmp] & (1UL<<_rem)) != 0;
}






static __inline__ __attribute__((always_inline)) void __FD_ZERO(__kernel_fd_set *p)
{
 unsigned long *tmp = (unsigned long *)p->fds_bits;
 int i;

 if (__builtin_constant_p((1024/(8 * sizeof(unsigned long))))) {
  switch ((1024/(8 * sizeof(unsigned long)))) {
        case 16:
   tmp[12] = 0; tmp[13] = 0; tmp[14] = 0; tmp[15] = 0;
   tmp[ 8] = 0; tmp[ 9] = 0; tmp[10] = 0; tmp[11] = 0;

        case 8:
   tmp[ 4] = 0; tmp[ 5] = 0; tmp[ 6] = 0; tmp[ 7] = 0;

        case 4:
   tmp[ 0] = 0; tmp[ 1] = 0; tmp[ 2] = 0; tmp[ 3] = 0;
   return;
  }
 }
 i = (1024/(8 * sizeof(unsigned long)));
 while (i) {
  i--;
  *tmp = 0;
  tmp++;
 }
}
# 48 "include/linux/posix_types.h" 2
# 12 "include/linux/types.h" 2
# 1 "/home/volatile/svnROACH/sw/borph_mainline_linux/arch/powerpc/include/asm/types.h" 1






# 1 "include/asm-generic/int-ll64.h" 1
# 17 "include/asm-generic/int-ll64.h"
typedef __signed__ char __s8;
typedef unsigned char __u8;

typedef __signed__ short __s16;
typedef unsigned short __u16;

typedef __signed__ int __s32;
typedef unsigned int __u32;


__extension__ typedef __signed__ long long __s64;
__extension__ typedef unsigned long long __u64;
# 40 "include/asm-generic/int-ll64.h"
typedef signed char s8;
typedef unsigned char u8;

typedef signed short s16;
typedef unsigned short u16;

typedef signed int s32;
typedef unsigned int u32;

typedef signed long long s64;
typedef unsigned long long u64;
# 8 "/home/volatile/svnROACH/sw/borph_mainline_linux/arch/powerpc/include/asm/types.h" 2
# 28 "/home/volatile/svnROACH/sw/borph_mainline_linux/arch/powerpc/include/asm/types.h"
typedef unsigned short umode_t;


typedef struct {
 __u32 u[4];
} __attribute__((aligned(16))) __vector128;
# 49 "/home/volatile/svnROACH/sw/borph_mainline_linux/arch/powerpc/include/asm/types.h"
typedef __vector128 vector128;



typedef u64 phys_addr_t;







typedef u32 dma_addr_t;

typedef u64 dma64_addr_t;

typedef struct {
 unsigned long entry;
 unsigned long toc;
 unsigned long env;
} func_descr_t;
# 13 "include/linux/types.h" 2



typedef __u32 __kernel_dev_t;

typedef __kernel_fd_set fd_set;
typedef __kernel_dev_t dev_t;
typedef __kernel_ino_t ino_t;
typedef __kernel_mode_t mode_t;
typedef __kernel_nlink_t nlink_t;
typedef __kernel_off_t off_t;
typedef __kernel_pid_t pid_t;
typedef __kernel_daddr_t daddr_t;
typedef __kernel_key_t key_t;
typedef __kernel_suseconds_t suseconds_t;
typedef __kernel_timer_t timer_t;
typedef __kernel_clockid_t clockid_t;
typedef __kernel_mqd_t mqd_t;


typedef _Bool bool;

typedef __kernel_uid32_t uid_t;
typedef __kernel_gid32_t gid_t;
typedef __kernel_uid16_t uid16_t;
typedef __kernel_gid16_t gid16_t;

typedef unsigned long uintptr_t;
# 57 "include/linux/types.h"
typedef __kernel_loff_t loff_t;
# 66 "include/linux/types.h"
typedef __kernel_size_t size_t;




typedef __kernel_ssize_t ssize_t;




typedef __kernel_ptrdiff_t ptrdiff_t;




typedef __kernel_time_t time_t;




typedef __kernel_clock_t clock_t;




typedef __kernel_caddr_t caddr_t;



typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;


typedef unsigned char unchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;




typedef __u8 u_int8_t;
typedef __s8 int8_t;
typedef __u16 u_int16_t;
typedef __s16 int16_t;
typedef __u32 u_int32_t;
typedef __s32 int32_t;



typedef __u8 uint8_t;
typedef __u16 uint16_t;
typedef __u32 uint32_t;


typedef __u64 uint64_t;
typedef __u64 u_int64_t;
typedef __s64 int64_t;
# 142 "include/linux/types.h"
typedef unsigned long sector_t;
# 151 "include/linux/types.h"
typedef unsigned long blkcnt_t;
# 180 "include/linux/types.h"
typedef __u16 __le16;
typedef __u16 __be16;
typedef __u32 __le32;
typedef __u32 __be32;

typedef __u64 __le64;
typedef __u64 __be64;

typedef __u16 __sum16;
typedef __u32 __wsum;


typedef unsigned gfp_t;


typedef u64 resource_size_t;




struct ustat {
 __kernel_daddr_t f_tfree;
 __kernel_ino_t f_tinode;
 char f_fname[6];
 char f_fpack[6];
};
# 355 "/home/volatile/svnROACH/sw/borph_mainline_linux/arch/powerpc/include/asm/unistd.h" 2

# 1 "include/linux/linkage.h" 1




# 1 "/home/volatile/svnROACH/sw/borph_mainline_linux/arch/powerpc/include/asm/linkage.h" 1
# 6 "include/linux/linkage.h" 2
# 357 "/home/volatile/svnROACH/sw/borph_mainline_linux/arch/powerpc/include/asm/unistd.h" 2
# 17 "arch/powerpc/kernel/systbl_chk.c" 2
# 56 "arch/powerpc/kernel/systbl_chk.c"
START_TABLE
# 1 "/home/volatile/svnROACH/sw/borph_mainline_linux/arch/powerpc/include/asm/systbl.h" 1





0
1
2
3
4
5
6
7
8
9
10
11
12
13
14
15
16
-1
18
-1
20
21
-1
23
24
25
26
27
28
29
30
-1
-1
33
34
-1
36
37
38
39
40
41
42
43
-1
45
46
47
48
49
50
51
52
-1
54
55
-1
57
-1
-1
60
61
62
63
64
65
66
67
68
69
70
71
72
73
74
75
76
77
78
79
80
81
-1
83
84
85
86
87
88
-1
90
91
92
93
94
95
96
97
-1
99
100
-1
102
103
104
105
106
107
108
-1
-1
111
-1
-1
114
115
116
117
118
119
120
121
122
-1
124
125
-1
-1
128
129
-1
131
132
133
134
135
-1
-1
138
139
140
141
-1
143
144
145
146
147
148
149
150
151
152
153
154
155
156
157
158
159
160
161
162
163
164
165
-1
167
168
169
170
171
172
173
174
175
176
177
178
179
180
181
182
183
184
185
-1
-1
-1
189
190
191
192
193
194
-1
-1
-1
198
199
200
-1
202
203
-1
205
206
207
208
209
210
211
212
213
214
215
216
217
218
219
220
221
222
223
-1
-1
226
227
228
229
230
231
232
-1
234
-1
236
237
238
239
-1
241
242
243
244
245
246
247
248
-1
250
251
252
253
-1
255
256
-1
258
259
260
261
262
263
264
265
266
267
268
269
270
271
272
273
274
275
276
277
278
279
280
281
282
283
284
285
286
287
288
289
290
-1
292
293
294
295
296
297
298
299
300
301
302
303
304
305
306
307
308
309
310
311
312
313
314
315
316
317
318
# 58 "arch/powerpc/kernel/systbl_chk.c" 2
END_TABLE 319
