This assignment entails processing road signs so that each pixel on the road sign is classified correctly as red (around the outside and sometimes through the middle), white or black. 
This is a preprocessing step to the recognition of these road signs. In order to classify the points correctly we must reliably identify the red points so that they encompass the rest 
of the sign and then need to do optimal thresholding on the contents of the sign (as opposed to the complete image). This is a little tricky as it involves finding the connected components 
for both the red parts of the signs and the contents of the sign, and then optimally thresholding the contents of each sign in turn.

I decided to implement template matching to solve the road sign detection problem. My solution
works quite well and matches all templates correctly except one (confuses a right arrow with left arrow - but the red point image 
isn't very clear) and it misses one - the parking symbol in the first realroadsigns image. This is because the arrows are slightly
different to that of the one in the templates.
