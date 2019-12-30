# vumeter 2.0

This is a true VU meter for PulseAudio. (Most so-called VU meters are in
fact peak program meters with a damped decay.)

A VU meter's reading corresponds approximately to the perceptual response of
the human ear. This implementation differs from an analog VU meter in that
it has a wider effective dynamic range and a full-scale value corresponding
to digital 0 dB. The meter's ballistic property matches an analog VU meter
in that it takes the needle approximately 300 milliseconds to go from
no-signal to 0 dB upon application of a full-scale signal.

It is important that you understand that this meter is not suited to setting
levels for a digital recording; use a peak program meter. This meter is best
suited for evaluating perceptual program levels during playback.

This code descends from original work by Martin Cameron. I've added text for
labels on the scale and a logo, and have reworked the initialization code to
(IMO) improve readability.

The program requires the SDL2 and SDL2\_ttf libraries and the DejaVu fonts.
