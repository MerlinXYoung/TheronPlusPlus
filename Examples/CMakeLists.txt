
include_directories(${PROJECT_SOURCE_DIR}/Utility)
include_directories(${PROJECT_SOURCE_DIR}/Communication)
include_directories(${PROJECT_SOURCE_DIR}/Communication/ZMQ)
include_directories(${PROJECT_SOURCE_DIR}/Communication/AMQ)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(HelloWorld HelloWorld.cpp)
target_link_libraries(HelloWorld Theron++ Threads::Threads)

add_executable(ScheduledHelloWorld ScheduledHelloWorld.cpp Scheduled.cpp)
target_link_libraries(ScheduledHelloWorld Theron++ gmpxx gmp Threads::Threads)

# add_executable(Serialisation Serialisation.cpp)
# target_link_libraries(Serialisation Theron++ Threads::Threads)

add_executable(ZMQDistribution ZMQDistribution.cpp)
target_link_libraries(ZMQDistribution Theron++ Threads::Threads)