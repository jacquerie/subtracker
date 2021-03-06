#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import cv2
import logging
import numpy

logger = logging.getLogger(__name__)

RECTANGLE_POINTS = numpy.array([(-1.0, 1.0),
                                (-1.0, -1.0),
                                (1.0, 1.0),
                                (1.0, -1.0)],
                               dtype=numpy.float32)

def image_corners(width, height):
    """Returns the coordinates in pixel of the corners of an image
    with given size, taking into account that each pixel
    is a square of side 1 centered in the point identified
    by its coordinates.
    
    The result is a numpy array containing the points:
        (-0.5, -0.5)
        (-0.5, height - 0.5)
        (width - 0.5, -0.5)
        (width - 0.5, height - 0.5)
    """
    return numpy.array([(-0.5, -0.5),
                        (-0.5, height - 0.5),
                        (width - 0.5, -0.5),
                        (width - 0.5, height - 0.5)], dtype=numpy.float32);

def scale(x, y):
    """Returns a 3x3 projecective matrix that scales x and y axes with the given factors"""
    return numpy.float32([
        [x, 0, 0],
        [0, y, 0],
        [0, 0, 1]
    ])

def translate(dx, dy):
    """Returns a 3x3 projecective matrix that translates along the given vector"""
    return numpy.float32([
        [1, 0,dx],
        [0, 1,dy],
        [0, 0, 1]
    ])

def pixels_to_rectangle(width, height):
    """Returns a 3x3 projective matrix that maps an image with given size
    to a rectangle, in a way such that the boundary of the rectangle
    exactly coincides with the boundary of the outermost pixels.

    More specifically, the matrix will map:
        (-1/2, -1/2)                |--> (-1, +1)
        (-1/2, height - 1/2)        |--> (-1, -1)
        (width - 1/2, -1/2)         |--> (+1, +1)
        (width - 1/2, height - 1/2) |--> (+1, -1)

    """
    return cv2.getPerspectiveTransform(image_corners(width, height), RECTANGLE_POINTS)

def rectangle_to_pixels(width, height):
    """Returns the inverse of pixels_to_rectangle().

    """
    return cv2.getPerspectiveTransform(RECTANGLE_POINTS, image_corners(width, height))

def rectangle_to_region(corners):
    """Return a 3x3 projective matrix that maps a rectangle to the region
    described by four corners.

    More specifically, the matrix will map:
        (-1, -1) |--> first corner
        (+1, -1) |--> second corner
        (+1, +1) |--> third corner
        (-1, +1) |--> fourth corner

    """
    corners = numpy.vstack([corners[3],
                            corners[0],
                            corners[2],
                            corners[1]])
    #logger.info("%r", corners)
    #logger.info("%r", RECTANGLE_POINTS)
    return cv2.getPerspectiveTransform(RECTANGLE_POINTS, corners)
    
def region_to_rectangle(corners):
    """Returns the inverse of rectangle_to_region(corners)

    """
    corners = numpy.vstack([corners[3],
                            corners[0],
                            corners[2],
                            corners[1]])
    #logger.info("%r", corners)
    #logger.info("%r", RECTANGLE_POINTS)
    return cv2.getPerspectiveTransform(corners, RECTANGLE_POINTS)

def apply_projectivity(projectivity, point):
    """
    Returns the transformed point.
    The projectivity must be a 3x3 matrix. Both input and output points are 2-dimensional.
    """
    p = numpy.dot(projectivity, numpy.array([point[0], point[1], 1.0]))
    return p[:2] / p[2]



