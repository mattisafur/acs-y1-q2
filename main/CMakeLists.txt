idf_component_register(
    SRCS
     "accelerometer.c"
     "app_wifi.c"
     "buzzer.c"
     "card_reader.c"
     "main.c"
     "metrics_publisher.c"
     "queue.c"
     "task_orchastrator.c"
     "time_of_flight.c"
     "time_sync.c"
    
    INCLUDE_DIRS "."
    
    REQUIRES
        esp_driver_i2c
        esp_driver_uart
        esp_http_client
        esp_netif
        esp_wifi
        json
        nvs_flash
        vl53l1x_library
)
