#ifndef _terra_next_h_
#define _terra_next_h_

/*
    Proposal for a code refactor with a more consistent notation that
    reflects the classical notation [Veach 1997] and its reintroduction in
    recent research [Novak 2018].

    - Consistent math/code notation
    - Prevents error

    Terra is a collection of c99 Monte Carlo path-tracing routines for light transport
    simulation.

    Also

# integration problem

X ~ Sample path
x ~ Element in P
P ~ Space of possible paths


estimators <I> = /d(x)/

    X

# estimators
    Estimators obtain the path contribution is transformed into display rgb Estimating the path contributioThe radiance integral you need  Monte Carlo estimator, which replace the estimator currently
    in use with one with less variance. They are grouped hierarchically for  The following distinctions are made.
        - Generic expected value estimators replace the random  replace the random variable sampling with a possibly lower
        variance estimator which
            - variance_reduction_stratified()
            - variance_reduction_latin_hypercube()
            - variance_reduction_mv_sobol()
            - variance_reduction_mv_reald()
            - variance_reduction_ multiple importance sampling
            - variance_reduction_rr()

        - local expected value estimators, they evaluate the scattering equation for a neighborhood
          of a path vertex, expanding
            Lo \approx
            Lo = //

          at a specific vertex. The difference from the ones above is that they have
          a-priori knowledge of the fact that they are solving the light path intregral.
          They are the ones responsible for issuing scene traversal queries.
          They are local though and not aware of the simulation status
            - path_estimator_split        | scattering equation, volumetric
            - path_path_prev    | previous vertex (recursive unidirectional)
            - path_camera       | camera sensor (pbr camera)
            - path_bsdf vertex | (importance sampling)
            - path_emissive     | emissive vertex bsdf (direct lighting)
            - path_merge        | light path vertex merging (bidirectional light transport)
            - path_density      |

    Radiance estimators solve the light path integral using a combination of the estimators
    above and possibly merging more paths. The ones below are obviously still Monte Carlo estimators,
    but are aware of the simulation status and can use different (possibly dynamic) strategies
    to reduce the variance in the radiance estimation.
            - radiance_integrate_path_backward
            - radiance_integrate_path_backward_volumetric
            - radiance_integrate_path_bidirectional
            - radiance_integrate_path_gradient
            - radiance_integrate_path_gradient_bidirectional
            - radiance_integrate_v

# reconstruction
    Once the radiance radiance going throuhas been estimated at a 3d point in the scene, it needs to be
    evaluated for an area on the sensor before being transformed to discrete pixel. The camera sensor connection
    { } is thus filtered with the following:
        - radiance_filter_gaussian
        - radiance_filter_box
        - radiance_filter_tent

    and converted to final tristimulus response for indirect human visualization:
        - sensor_film_linear
        - sensor_film_srgb
        - sensor_film_tonemap_
        - sensor_film_

# ray tracing
    The scene traversal queries issued by local path estimators have:
        - ray/primitive intersection: moller-trumblor [], pluecker coordinates []
        - 1-primitive, 4-primitive and 8-primitive vectorized routines for SSE3+ architectures
        - Spatial acceleration structures for single-ray traversal scene traversal with branching:
            - bvh (n logn sah binning) construction []
            - multi-branching bvh (1/4/8-primitive tests on leaf nodes) []
            - multi-bvh ray stream (1/4/8-primitive tests on leaf and inner nodes) []
        - Sensor footprint differentials []

# Radiometric quantities


*/

#endif // _terra_next_h_