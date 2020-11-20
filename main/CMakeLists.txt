idf_component_register( SRCS    "main.c"
                        INCLUDE_DIRS "." "../components"
                        PRIV_REQUIRES "transport" "nvs_flash")
