idf_component_register(SRCS "main.cpp" "mqtt_queue.cpp"
                    INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-missing-field-initializers)
