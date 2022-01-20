// trick_classes.h

// An igen kludge to handle pointers to abstract base classes.

#ifndef _TRICK_CLASSES_H_
#define _TRICK_CLASSES_H_

//---------------------

class monotone_bitwise_dataflow_fn;

class trick_mbdf {
 private:
   const monotone_bitwise_dataflow_fn *fn;

   trick_mbdf& operator=(const trick_mbdf &);

 public:
   trick_mbdf(const trick_mbdf &);
   trick_mbdf(const monotone_bitwise_dataflow_fn *ifn);
  ~trick_mbdf();

   const monotone_bitwise_dataflow_fn* get() const;
};

//---------------------

class function_t;

class trick_fn {
 private:
   const function_t *fn;

   trick_fn(const trick_fn &);
   trick_fn& operator=(const trick_fn &);
   
 public:
   trick_fn(const function_t *);
  ~trick_fn();

   const function_t* get() const;
};

//---------------------

class regset_t;

class trick_regset {
 private:
   const regset_t *rs;

   trick_regset& operator=(const trick_regset &);
   
 public:
   trick_regset();
   trick_regset(const trick_regset &);
   trick_regset(const regset_t *);
  ~trick_regset();

   const regset_t* get() const;
};

//---------------------

class relocatableCode_t;

class trick_relocatableCode {
 private:
   const relocatableCode_t *rc;

   trick_relocatableCode& operator=(const trick_relocatableCode &);
   
 public:
   trick_relocatableCode(const trick_relocatableCode &);
   trick_relocatableCode(const relocatableCode_t *);
  ~trick_relocatableCode();

   const relocatableCode_t* get() const;
};

#endif //TRICK_CLASSES_H
