#include "loadedModule.h"
#include "kapi.h"

kapi_module::kapi_module(const loadedModule *mod, kapi_manager *mgr) : 
    theModule(mod), theManager(mgr)
{
}

unsigned kapi_module::getNumFunctions() const
{
    return theModule->numFns();
}

ierr kapi_module::findFunction(const char *name, kapi_function *kfunc) const
{
    pdvector<kptr_t> func_addr = theModule->find_fn_by_name(name);
    if (func_addr.size() == 0) {
	return function_not_found;
    }
    else if (func_addr.size() > 1) {
	return function_not_unique;
    }
    return theManager->findFunctionByAddress(func_addr[0], kfunc);
}

// Fill-in module's name
ierr kapi_module::getName(char *name, unsigned max_bytes) const
{
    pdstring mod_name = theModule->getName();
    unsigned len = mod_name.length();
    if (len+1 > max_bytes) {
	return not_enough_space;
    }
    strcpy(name, mod_name.c_str());
    return 0;
}

// Fill-in module's name
ierr kapi_module::getDescription(char *name, unsigned max_bytes) const
{
    pdstring mod_name = theModule->getDescription();
    unsigned len = mod_name.length();
    if (len+1 > max_bytes) {
	return not_enough_space;
    }
    if (len > 0) {
	strcpy(name, mod_name.c_str());
    }
    else {
	name[0] = 0; // .c_str of an empty pdstring is null
    }
    return 0;
}

// Return a vector of all functions in the module
ierr kapi_module::getAllFunctions(kapi_vector<kapi_function> *vec) const

{
    loadedModule::const_iterator iter = theModule->begin();
    loadedModule::const_iterator finish = theModule->end();
    for (; iter != finish; iter++) {
	vec->push_back(kapi_function(*iter));
    }
    return 0;
}


