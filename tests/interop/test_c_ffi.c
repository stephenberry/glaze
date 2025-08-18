// Simple C test to demonstrate pure C FFI using existing infrastructure
#include "glaze/interop/interop_c.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

// Simple test struct and functions
typedef struct {
    int value;
} TestStruct;

void* create_test_struct(void) {
    TestStruct* ts = malloc(sizeof(TestStruct));
    ts->value = 42;
    return ts;
}

void destroy_test_struct(void* ptr) {
    free(ptr);
}

void* get_value(void* obj) {
    return &((TestStruct*)obj)->value;
}

void set_value(void* obj, void* value) {
    ((TestStruct*)obj)->value = *(int*)value;
}

int main() {
    printf("Testing C FFI with existing glaze infrastructure...\n");
    
    // Test type registration using C FFI
    bool success = glz_register_type_dynamic(
        "TestStruct",
        sizeof(TestStruct),
        sizeof(TestStruct), // alignment
        create_test_struct,
        destroy_test_struct
    );
    assert(success);
    printf("✓ Type registration works\n");
    
    // Test member registration
    success = glz_register_member_data(
        "TestStruct",
        "value",
        get_value,
        set_value
    );
    assert(success);
    printf("✓ Member registration works\n");
    
    // Test type info retrieval (uses existing C API)
    glz_type_info* type_info = glz_get_type_info("TestStruct");
    assert(type_info != NULL);
    assert(type_info->member_count == 1);
    printf("✓ Type info retrieval works (found %zu members)\n", type_info->member_count);
    
    // Test instance creation (uses existing C API)
    void* instance = glz_create_instance("TestStruct");
    assert(instance != NULL);
    TestStruct* ts = (TestStruct*)instance;
    assert(ts->value == 42);
    printf("✓ Instance creation works (value = %d)\n", ts->value);
    
    // Cleanup (uses existing C API)
    glz_destroy_instance("TestStruct", instance);
    
    printf("\n🎉 C FFI test passed!\n");
    printf("✅ Pure C functions integrate with existing glaze infrastructure\n");
    
    return 0;
}