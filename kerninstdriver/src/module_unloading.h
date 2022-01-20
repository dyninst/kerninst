/* module_unloading.h */

#ifndef _MODULE_UNLOADING_H_
#define _MODULE_UNLOADING_H_

#ifdef __cplusplus
extern "C" {
   void kerninst_modunload_disable();
   void kerninst_modunload_enable();
}
#else
   void kerninst_modunload_disable(void);
   void kerninst_modunload_enable(void);
#endif

#endif
