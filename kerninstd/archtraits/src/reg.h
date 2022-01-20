// reg.h

#ifndef _REG_H_
#define _REG_H_

#include "common/h/Vector.h"
#include "uimm.h"
#include "common/h/String.h"

class reg_t {
 protected:
   unsigned regnum;
   
   bool operator<(const reg_t &src) const;
   bool operator<=(const reg_t &src) const;
   bool operator>=(const reg_t &src) const;
   bool operator>(const reg_t &src) const;

   //virtual void disass_int(char *) const = 0;
   //virtual void disass_float(char *) const = 0;
   //virtual void disass_misc(char *) const = 0;

   reg_t(const reg_t &src);
   reg_t(unsigned inum);

 public:
   virtual ~reg_t();

   static const unsigned getMaxRegNum();
   static reg_t& getRegByBit(unsigned);

   bool operator==(const reg_t &src) const;
   bool operator!=(const reg_t &src) const;

   reg_t& operator=(const reg_t &src);

   virtual bool isIntReg() const = 0;
   virtual bool isFloatReg() const = 0;
   virtual bool isIntCondCode() const = 0;
   virtual bool isFloatCondCode() const = 0;
   virtual bool isPC() const = 0;

   virtual unsigned getIntNum() const = 0;
   virtual unsigned getFloatNum() const = 0;

   virtual pdstring disass() const = 0;
};

#endif /*_ARCH_REG_H_*/
