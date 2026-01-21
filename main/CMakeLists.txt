idf_component_register(
    SRCS "main.c"
         "queue.c"
         "accelerometer.c"
         "buzzer.c"
         "card_reader.c"
         "metrics_publisher.c"
         "task_orchastrator.c"
        
    INCLUDE_DIRS "."
)
