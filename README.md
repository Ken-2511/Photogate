# Photogate

## Background

Photogates are used to measure objects' speed or acceleration. My high school physics teacher wanted a teaching aid to demonstrate the objects' speed change. In her imagination, there are ten photogates forming a tunnel, and an object go through the photogates, and the photogates show the time of seeing the object.

## My Design Specification

Here is my design specification.

### How to Detect Object

I decided to use laser emitter and receiver to detect objects' movement. Whenever an opaque object goes through the photogate, the laser is blocked, and the laser receiver would push a signal to the microprocessor (Arduino-nano).
<div align="center">
<img src="https://github.com/Ken-2511/Photogate/blob/main/images/nixie_tube_feature.jpg" width=400/>
</div>

### How to Show the Time

I installed a nixie-tube on each photogate to show their time (Figure 1) and the nixie-tube is controlled by an Arduino-nano.

### How to Adjust Time

Since Arduino has no time-adjusting function, and the time-measuring job requires a very precise timing: the time error should be less than 1 millisecond. Thus, we should better not use bluetooth or WiFi to adjust time.

In this project, I used infrared remote controlling technique, similar to TV controller or AC controller.

However, the existing libraries does not fulfill our need as they are memory-consuming and low-efficient. Most libraries only support to transmit 8 bits per time. Therefore, I developed my own "protocol" to allow it transmit up to 32 bytes (256 bits) per time (they are defined in "photogate.c" and "PhotogateController.c").

There is a "photogate controller", as the name says, it controls all the ten photogates by sending infrared signals (Figure 2). We use the controller's internal timer as the "standard time", and each photogate adjust time and stores the difference between their timers into their memory. Since the time difference between each timer does not vary, we got a relatively precise timing system.

By applying this technique, we finally got the time-adjusting system with lower than 150 microseconds (0.00015 seconds) precision, which is adequate for being a teaching aid.

### User-Friendly Consideration

To make it usable by a non-programmer, I designed a graphoc user interface (GUI) using Python Tkinter library.

### Physical Design Specification

The overall structure is built by aluminum beams. To make it more stable (not easy to fall down), I 3D printed some kind of stands and stick it on the photogates.

This design uses many electronic devices, including voltage-boosting module, battery controlling module, switches, and nixie tube.... To integrade these electronic modules together, I designed a PCB and soldered them together. I also printed a tiny box to hold electronics and fix them to the aluminum frame.

The laser does not always accurately shoot on the laser receiver. Therefore, I designed a "fine-tuning" structure. By turning the screw, we can slightly adjust the laser's direction.

For further information, you can check the .docx file.
