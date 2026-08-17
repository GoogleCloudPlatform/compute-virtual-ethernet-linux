@@
identifier skb, header_len;
@@

+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,0,0))
header_len = skb_tcp_all_headers(skb);
+#else
+header_len = skb_transport_offset(skb) + tcp_hdrlen(skb);
+#endif


@@
identifier skb, header_len;
@@

+#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,0,0))
const int header_len = skb_tcp_all_headers(skb);
+#else
+const int header_len = skb_checksum_start_offset(skb) + tcp_hdrlen(skb);
+#endif

/* Skipping GRO header linearization in kunit kernel as few of the methods
 * used don't exist in our very old kunit kernel and flow-disector logic is
 * severely outdated.
 */
@@
@@
+#if !defined(KUNIT_KERNEL)
skb_copy_to_linear_data(skb, va, hdr_len);
...
skb->tail += hdr_len;
+#endif