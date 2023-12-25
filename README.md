# Photogate

## Background

Photogates are used to measure objects' speed or acceleration. My high school physics teacher wanted a teaching aid to demonstrate the objects' speed change. In her imagination, there are ten photogates forming a tunnel, and an object go through the photogates, and the photogates show the time of seeing the object.

## My Design Specification

Here is my design specification.

### How to detect object

I decided to use laser emitter and receiver to detect objects' movement. Whenever an opaque object goes through the photogate, the laser is blocked, and the laser receiver would push a signal to the microprocessor (Arduino-nano).

### How to show the time

I installed a nixie-tube on each photogate to show their time (Figure 1) and the nixie-tube is controlled by an Arduino-nano.

### How to adjust time

Since Arduino has no time-adjusting function, and the time-measuring job requires a very precise timing: the time error should be less than 1 millisecond. Thus, we should better not use bluetooth or WiFi to adjust time.

In this project, I used infrared remote controlling technique, similar to TV controller or AC controller.

However, the existing libraries does not fulfill our need as they are memory-consuming and low-efficient. Most libraries only support to transmit 8 bits per time. Therefore, I developed my own "protocol" to allow it transmit up to 32 bytes (256 bits) per time (they are defined in "photogate.c" and "PhotogateController.c").

There is a "photogate controller", as the name says, it controls all the ten photogates by sending infrared signals (Figure 2). We use the controller's internal timer as the "standard time", and each photogate adjust time and stores the difference between their timers into their memory. Since the time difference between each timer does not vary, we got a relatively precise timing system.

By applying this technique, we finally got the time-adjusting system with lower than 150 microseconds (0.00015 seconds) precision, which is adequate for being a teaching aid.

### User-friendly Consideration

To make it easy to use, or usable by a non-programmer, I designed a graphoc user interface (GUI) using Python Tkinter library.

There are lower computer and upper computer programs for the "photogate" designed for my high school.
