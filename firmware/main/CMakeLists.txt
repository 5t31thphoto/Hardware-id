idf_component_register(SRCS "main.cpp" INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error=unused-function -Wno-error=unused-variable
    -Wno-error=sign-compare -Wno-error=missing-field-initializers)
