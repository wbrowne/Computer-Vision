Given a video of some postboxes which have a regular pattern in the base of each postbox, check if there is post in each postbox.

My program successfully determines the correct status of each postbox. It does this by scanning each postbox horizontally and determining how many white points (vertical edges) it hits. 
If this whitepoints_hit_count is below a certain amount, I say that there must be post in that particular box. I had begun to start implementing a tracing algorithm to do the vertical edge
analysis instead but didn't get very far unfortunately. If I had more time I would have used the graph searching algorithm.

Note: extra image added to attached files to show use of a different sobel method
