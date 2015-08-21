  * [Fast non-maximal suppression](http://www.vision.ee.ethz.ch/~aneubeck/)
  * [SURF author homepage](http://www.vision.ee.ethz.ch/~surf/)
  * [SURF paper (pdf)](ftp://ftp.vision.ee.ethz.ch/publications/articles/eth_biwi_00517.pdf)
  * From the paper, there are overlapping filters performed at different octaves; by having a translation table it is possible to avoid recomputing some levels in the pyramid.
  * [Detailed notes about OpenSURF's implementation](http://www.cs.bris.ac.uk/Publications/pub_master.jsp?id=2000970). Or, see the [pdf](http://www.jmkirby.co.uk/surf.pdf) (also [pdf](http://www.cs.bris.ac.uk/Publications/Papers/2000970.pdf)).
  * [Brown & Lowe paper describing how to refining local maxima in scalespace](http://www.comp.leeds.ac.uk/bmvc2008/proceedings/2002/papers/92/full_92.pdf).
  * [Test data sets (graffiti)](http://www.robots.ox.ac.uk/~vgg/research/affine/).

## Code location ##

  * image/blob\_response.h - Hessian response (interest point detection).
  * image/integral\_image.h - The summed-area-table.
  * image/non\_maximal\_suppression.h - Find local maxima in an Array3Df.

#### Stuff that's not finished ####

  * image/surf.h - Overall algorithm; calls other pieces. Does interest point detection at different scales but doesn't calculate descriptors.
  * image/surf\_description.h - (not started) Calculate a surf description given an image, scale, and location.
  * image/kdtree.h - (not started) N-dimensional matching. May switch to LSH, depending.