/* List each unit test as selftest(name, function)
 *
 * The name is used as both an enum and expanded as subtest__name to create
 * a module parameter. It must be unique and legal for a C identifier.
 *
 * The function should be of type int function(void). It may be conditionally
 * compiled using #if IS_ENABLED(DRM_I915_SELFTEST).
 *
 * Tests are executed in order by igt/drv_selftest
 */
selftest(sanitycheck, i915_mock_sanitycheck) /* keep first (igt selfcheck) */
selftest(scatterlist, scatterlist_mock_selftests)
selftest(uncore, intel_uncore_mock_selftests)
selftest(breadcrumbs, intel_breadcrumbs_mock_selftests)
selftest(requests, i915_gem_request_mock_selftests)
selftest(objects, i915_gem_object_mock_selftests)
selftest(dmabuf, i915_gem_dmabuf_mock_selftests)
selftest(vma, i915_vma_mock_selftests)
selftest(evict, i915_gem_evict_mock_selftests)
selftest(gtt, i915_gem_gtt_mock_selftests)
