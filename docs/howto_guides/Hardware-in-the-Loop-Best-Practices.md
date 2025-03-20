# Trick Hardware in the Loop Best Practices

**Contents**

* [Purpose](#Purpose)<br>
* [Prerequisite Knowledge](#prerequisite-knowledge)<br>
* [Do's, Don'ts and Wisdom](#guidelines)<br>

<a id= introduction></a>

---
## Purpose
The intention of this document is to compile and share practical knowledge, based on the experience of people in the Trick simulation community regarding the development of hardware in the loop computer simulations.

<a id=prerequisite-knowledge></a>
## Prerequisite Knowledge

* (Assuming you've completed the [Trick Tutorial](https://nasa.github.io/trick/tutorial/Tutorial))
* (Assuming you've read the [Realtime Best Practices](https://nasa.github.io/trick/docs/howto_guides/Realtime-Best-Practices.md))

---
## The Main Take a Way

The most important thing for a develper to know is <b>When What</b> has to be <b>Where</b>.

* When is at what time is the data expected.
* What is what data needs to be moved.
* Where is the hardware that needs the data. 

### Trick Features
#### Trick Child Threads are your Friends
#### Trick Command Line Arguments
### Logging
#### send_hs is your Friend
### Devloping
#### You should build your code in a Dynamic libaray (lib.so) file
### From the Development envirement into the Lab
