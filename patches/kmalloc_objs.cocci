// ==========================================
// kmalloc_obj
// ==========================================
@@
expression VAR;
type T;
expression FLAGS;
@@
(
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kmalloc_obj(T, FLAGS);
+#else
+VAR = kmalloc(sizeof(*VAR), FLAGS);
+#endif
|
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kmalloc_obj(T);
+#else
+VAR = kmalloc(sizeof(*VAR), GFP_KERNEL);
+#endif
)

@@
expression VAR;
expression E;
expression FLAGS;
@@
(
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kmalloc_obj(E, FLAGS);
+#else
+VAR = kmalloc(sizeof(*VAR), FLAGS);
+#endif
|
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kmalloc_obj(E);
+#else
+VAR = kmalloc(sizeof(*VAR), GFP_KERNEL);
+#endif
)

// ==========================================
// kzalloc_obj
// ==========================================
@@
expression VAR;
type T;
expression FLAGS;
@@
(
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kzalloc_obj(T, FLAGS);
+#else
+VAR = kzalloc(sizeof(*VAR), FLAGS);
+#endif
|
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kzalloc_obj(T);
+#else
+VAR = kzalloc(sizeof(*VAR), GFP_KERNEL);
+#endif
)

@@
expression VAR;
expression E;
expression FLAGS;
@@
(
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kzalloc_obj(E, FLAGS);
+#else
+VAR = kzalloc(sizeof(*VAR), FLAGS);
+#endif
|
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kzalloc_obj(E);
+#else
+VAR = kzalloc(sizeof(*VAR), GFP_KERNEL);
+#endif
)

// ==========================================
// kvzalloc_obj
// ==========================================
@@
expression VAR;
type T;
expression FLAGS;
@@
(
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kvzalloc_obj(T, FLAGS);
+#else
+VAR = kvzalloc(sizeof(*VAR), FLAGS);
+#endif
|
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kvzalloc_obj(T);
+#else
+VAR = kvzalloc(sizeof(*VAR), GFP_KERNEL);
+#endif
)

@@
expression VAR;
expression E;
expression FLAGS;
@@
(
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kvzalloc_obj(E, FLAGS);
+#else
+VAR = kvzalloc(sizeof(*VAR), FLAGS);
+#endif
|
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kvzalloc_obj(E);
+#else
+VAR = kvzalloc(sizeof(*VAR), GFP_KERNEL);
+#endif
)

// ==========================================
// kvmalloc_obj
// ==========================================
@@
expression VAR;
type T;
expression FLAGS;
@@
(
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kvmalloc_obj(T, FLAGS);
+#else
+VAR = kvmalloc(sizeof(*VAR), FLAGS);
+#endif
|
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kvmalloc_obj(T);
+#else
+VAR = kvmalloc(sizeof(*VAR), GFP_KERNEL);
+#endif
)

@@
expression VAR;
expression E;
expression FLAGS;
@@
(
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kvmalloc_obj(E, FLAGS);
+#else
+VAR = kvmalloc(sizeof(*VAR), FLAGS);
+#endif
|
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kvmalloc_obj(E);
+#else
+VAR = kvmalloc(sizeof(*VAR), GFP_KERNEL);
+#endif
)

// ==========================================
// kmalloc_objs
// ==========================================
@@
expression VAR;
type T;
expression COUNT;
expression FLAGS;
@@
(
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kmalloc_objs(T, COUNT, FLAGS);
+#else
+VAR = kmalloc_array(COUNT, sizeof(*VAR), FLAGS);
+#endif
|
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kmalloc_objs(T, COUNT);
+#else
+VAR = kmalloc_array(COUNT, sizeof(*VAR), GFP_KERNEL);
+#endif
)

@@
expression VAR;
expression E;
expression COUNT;
expression FLAGS;
@@
(
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kmalloc_objs(E, COUNT, FLAGS);
+#else
+VAR = kmalloc_array(COUNT, sizeof(*VAR), FLAGS);
+#endif
|
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kmalloc_objs(E, COUNT);
+#else
+VAR = kmalloc_array(COUNT, sizeof(*VAR), GFP_KERNEL);
+#endif
)

// ==========================================
// kzalloc_objs
// ==========================================
@@
expression VAR;
type T;
expression COUNT;
expression FLAGS;
@@
(
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kzalloc_objs(T, COUNT, FLAGS);
+#else
+VAR = kcalloc(COUNT, sizeof(*VAR), FLAGS);
+#endif
|
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kzalloc_objs(T, COUNT);
+#else
+VAR = kcalloc(COUNT, sizeof(*VAR), GFP_KERNEL);
+#endif
)

@@
expression VAR;
expression E;
expression COUNT;
expression FLAGS;
@@
(
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kzalloc_objs(E, COUNT, FLAGS);
+#else
+VAR = kcalloc(COUNT, sizeof(*VAR), FLAGS);
+#endif
|
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kzalloc_objs(E, COUNT);
+#else
+VAR = kcalloc(COUNT, sizeof(*VAR), GFP_KERNEL);
+#endif
)

// ==========================================
// kvzalloc_objs
// ==========================================
@@
expression VAR;
type T;
expression COUNT;
expression FLAGS;
@@
(
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kvzalloc_objs(T, COUNT, FLAGS);
+#else
+VAR = kvcalloc(COUNT, sizeof(*VAR), FLAGS);
+#endif
|
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kvzalloc_objs(T, COUNT);
+#else
+VAR = kvcalloc(COUNT, sizeof(*VAR), GFP_KERNEL);
+#endif
)

@@
expression VAR;
expression E;
expression COUNT;
expression FLAGS;
@@
(
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kvzalloc_objs(E, COUNT, FLAGS);
+#else
+VAR = kvcalloc(COUNT, sizeof(*VAR), FLAGS);
+#endif
|
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kvzalloc_objs(E, COUNT);
+#else
+VAR = kvcalloc(COUNT, sizeof(*VAR), GFP_KERNEL);
+#endif
)

// ==========================================
// kvmalloc_objs
// ==========================================
@@
expression VAR;
type T;
expression COUNT;
expression FLAGS;
@@
(
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kvmalloc_objs(T, COUNT, FLAGS);
+#else
+VAR = kvmalloc_array(COUNT, sizeof(*VAR), FLAGS);
+#endif
|
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kvmalloc_objs(T, COUNT);
+#else
+VAR = kvmalloc_array(COUNT, sizeof(*VAR), GFP_KERNEL);
+#endif
)

@@
expression VAR;
expression E;
expression COUNT;
expression FLAGS;
@@
(
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kvmalloc_objs(E, COUNT, FLAGS);
+#else
+VAR = kvmalloc_array(COUNT, sizeof(*VAR), FLAGS);
+#endif
|
+#if LINUX_VERSION_CODE >= KERNEL_VERSION(7,0,0)
VAR = kvmalloc_objs(E, COUNT);
+#else
+VAR = kvmalloc_array(COUNT, sizeof(*VAR), GFP_KERNEL);
+#endif
)
