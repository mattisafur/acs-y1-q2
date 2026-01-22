idf_component_register(
    SRCS "accelerometer.c"
         "buzzer.c"
         "card_reader.c"
         "i2c_master_bus.c"
         "main.c"
         "metrics_publisher.c"
         "queue.c"
         "task_orchastrator.c"
         "time_of_flight.c"
    
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2c vl53l1x_library
)
