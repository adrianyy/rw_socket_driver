#include "berkeley.h"
#include "ksocket.h"

//////////////////////////////////////////////////////////////////////////
// Definitions.
//////////////////////////////////////////////////////////////////////////

#define MEMORY_TAG            ' bsK'
#define SOCKETFD_MAX          128
#define TO_SOCKETFD(index)    ((index % SOCKETFD_MAX)  + 1)
#define FROM_SOCKETFD(sockfd) ((sockfd)                - 1)

//////////////////////////////////////////////////////////////////////////
// Function prototypes.
//////////////////////////////////////////////////////////////////////////

NTSTATUS
NTAPI
KspUtilAddrInfoToAddrInfoEx(
  _In_ PADDRINFOA AddrInfo,
  _Out_ PADDRINFOEXW* AddrInfoEx
  );

NTSTATUS
NTAPI
KspUtilAddrInfoExToAddrInfo(
  _In_ PADDRINFOEXW AddrInfoEx,
  _Out_ PADDRINFOA* AddrInfo
  );

VOID
NTAPI
KspUtilFreeAddrInfo(
  _In_ PADDRINFOA AddrInfo
  );

VOID
NTAPI
KspUtilFreeAddrInfoEx(
  _In_ PADDRINFOEXW AddrInfo
  );

//////////////////////////////////////////////////////////////////////////
// Variables.
//////////////////////////////////////////////////////////////////////////

//
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!! NOTE !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!! NOTE !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!! NOTE !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//
// This is complete bollocks and ideally it should be replaced with
// something like RTL_AVL_TABLE.
//
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!! NOTE !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!! NOTE !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!! NOTE !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//

PKSOCKET KsArray[SOCKETFD_MAX] = { 0 };
ULONG    KsIndex = 0;

//////////////////////////////////////////////////////////////////////////
// Private functions.
//////////////////////////////////////////////////////////////////////////

NTSTATUS
NTAPI
KspUtilAddrInfoToAddrInfoEx(
  _In_ PADDRINFOA AddrInfo,
  _Out_ PADDRINFOEXW* AddrInfoEx
  )
{
  NTSTATUS Status;

  //
  // Convert NULL input into NULL output.
  //

  if (AddrInfo == NULL)
  {
    *AddrInfoEx = NULL;
    return STATUS_SUCCESS;
  }

  //
  // Allocate memory for the output structure.
  //

  PADDRINFOEXW Result = ExAllocatePoolWithTag(PagedPool, sizeof(ADDRINFOEXW), MEMORY_TAG);

  if (Result == NULL)
  {
    Status = STATUS_INSUFFICIENT_RESOURCES;
    goto Error1;
  }

  //
  // Copy numeric values.
  //

  RtlZeroMemory(Result, sizeof(ADDRINFOEXW));
  Result->ai_flags    = AddrInfo->ai_flags;
  Result->ai_family   = AddrInfo->ai_family;
  Result->ai_socktype = AddrInfo->ai_socktype;
  Result->ai_protocol = AddrInfo->ai_protocol;
  Result->ai_addrlen  = AddrInfo->ai_addrlen;

  //
  // Copy canonical name.
  //

  ANSI_STRING CanonicalNameAnsi;
  UNICODE_STRING CanonicalNameUnicode;

  if (AddrInfo->ai_canonname)
  {
    RtlInitAnsiString(&CanonicalNameAnsi, AddrInfo->ai_canonname);

    Status = RtlAnsiStringToUnicodeString(&CanonicalNameUnicode, &CanonicalNameAnsi, TRUE);

    if (!NT_SUCCESS(Status))
    {
      goto Error2;
    }

    Result->ai_canonname = CanonicalNameUnicode.Buffer;
  }

  //
  // Copy address.
  //

  Result->ai_addr = AddrInfo->ai_addr;

  //
  // Copy the next structure (recursively).
  //

  PADDRINFOEXW NextAddrInfo;
  Status = KspUtilAddrInfoToAddrInfoEx(AddrInfo->ai_next, &NextAddrInfo);

  if (!NT_SUCCESS(Status))
  {
    goto Error3;
  }

  Result->ai_next = NextAddrInfo;

  //
  // All done!
  //

  *AddrInfoEx = Result;

  return Status;

Error3:
  RtlFreeAnsiString(&CanonicalNameAnsi);

Error2:
  ExFreePoolWithTag(Result, MEMORY_TAG);

Error1:
  return Status;
}

NTSTATUS
NTAPI
KspUtilAddrInfoExToAddrInfo(
  _In_ PADDRINFOEXW AddrInfoEx,
  _Out_ PADDRINFOA* AddrInfo
  )
{
  NTSTATUS Status;

  //
  // Convert NULL input into NULL output.
  //

  if (AddrInfoEx == NULL)
  {
    *AddrInfo = NULL;
    return STATUS_SUCCESS;
  }

  //
  // Allocate memory for the output structure.
  //

  PADDRINFOA Result = ExAllocatePoolWithTag(PagedPool, sizeof(ADDRINFOA), MEMORY_TAG);

  if (Result == NULL)
  {
    Status = STATUS_INSUFFICIENT_RESOURCES;
    goto Error1;
  }

  //
  // Copy numeric values.
  //

  RtlZeroMemory(Result, sizeof(ADDRINFOA));
  Result->ai_flags    = AddrInfoEx->ai_flags;
  Result->ai_family   = AddrInfoEx->ai_family;
  Result->ai_socktype = AddrInfoEx->ai_socktype;
  Result->ai_protocol = AddrInfoEx->ai_protocol;
  Result->ai_addrlen  = AddrInfoEx->ai_addrlen;

  //
  // Copy canonical name.
  //

  UNICODE_STRING CanonicalNameUnicode;
  ANSI_STRING CanonicalNameAnsi;

  if (AddrInfoEx->ai_canonname)
  {
    RtlInitUnicodeString(&CanonicalNameUnicode, AddrInfoEx->ai_canonname);
    Status = RtlUnicodeStringToAnsiString(&CanonicalNameAnsi, &CanonicalNameUnicode, TRUE);

    if (!NT_SUCCESS(Status))
    {
      goto Error2;
    }

    Result->ai_canonname = CanonicalNameAnsi.Buffer;
  }

  //
  // Copy address.
  //

  Result->ai_addr = AddrInfoEx->ai_addr;

  //
  // Copy the next structure (recursively).
  //

  PADDRINFOA NextAddrInfo;
  Status = KspUtilAddrInfoExToAddrInfo(AddrInfoEx->ai_next, &NextAddrInfo);

  if (!NT_SUCCESS(Status))
  {
    goto Error3;
  }

  Result->ai_next = NextAddrInfo;

  //
  // All done!
  //

  *AddrInfo = Result;

  return Status;

Error3:
  RtlFreeAnsiString(&CanonicalNameAnsi);

Error2:
  ExFreePoolWithTag(Result, MEMORY_TAG);

Error1:
  return Status;
}

VOID
NTAPI
KspUtilFreeAddrInfo(
  _In_ PADDRINFOA AddrInfo
  )
{
  //
  // Free all structures recursively.
  //

  if (AddrInfo->ai_next)
  {
    KspUtilFreeAddrInfo(AddrInfo->ai_next);
  }

  //
  // Free the canonical name buffer.
  //

  if (AddrInfo->ai_canonname)
  {
    ANSI_STRING CanonicalName;
    RtlInitAnsiString(&CanonicalName, AddrInfo->ai_canonname);
    RtlFreeAnsiString(&CanonicalName);
  }

  //
  // Finally, free the structure itself.
  //

  ExFreePoolWithTag(AddrInfo, MEMORY_TAG);
}

VOID
NTAPI
KspUtilFreeAddrInfoEx(
  _In_ PADDRINFOEXW AddrInfo
  )
{
  //
  // Free all structures recursively.
  //

  if (AddrInfo->ai_next)
  {
    KspUtilFreeAddrInfoEx(AddrInfo->ai_next);
  }

  //
  // Free the canonical name buffer.
  //

  if (AddrInfo->ai_canonname)
  {
    UNICODE_STRING CanonicalName;
    RtlInitUnicodeString(&CanonicalName, AddrInfo->ai_canonname);
    RtlFreeUnicodeString(&CanonicalName);
  }

  //
  // Finally, free the structure itself.
  //

  ExFreePoolWithTag(AddrInfo, MEMORY_TAG);
}

//////////////////////////////////////////////////////////////////////////
// Public functions.
//////////////////////////////////////////////////////////////////////////

uint32_t htonl(uint32_t hostlong)
{
  return RtlUlongByteSwap(hostlong);
}

uint16_t htons(uint16_t hostshort)
{
  return RtlUshortByteSwap(hostshort);
}

uint32_t ntohl(uint32_t netlong)
{
  return RtlUlongByteSwap(netlong);
}

uint16_t ntohs(uint16_t netshort)
{
  return RtlUshortByteSwap(netshort);
}

int getaddrinfo(const char* node, const char* service, const struct addrinfo* hints, struct addrinfo** res)
{
  NTSTATUS Status;

  //
  // Convert node name to the UNICODE_STRING (if present).
  //

  ANSI_STRING NodeNameAnsi;
  UNICODE_STRING NodeNameUnicode;
  PUNICODE_STRING NodeName = NULL;

  if (node)
  {
    RtlInitAnsiString(&NodeNameAnsi, node);
    Status = RtlAnsiStringToUnicodeString(&NodeNameUnicode, &NodeNameAnsi, TRUE);

    if (!NT_SUCCESS(Status))
    {
      goto Error1;
    }

    NodeName = &NodeNameUnicode;
  }

  //
  // Convert service name to the UNICODE_STRING (if present).
  //

  ANSI_STRING ServiceNameAnsi;
  UNICODE_STRING ServiceNameUnicode;
  PUNICODE_STRING ServiceName = NULL;

  if (service)
  {
    RtlInitAnsiString(&ServiceNameAnsi, service);
    Status = RtlAnsiStringToUnicodeString(&ServiceNameUnicode, &ServiceNameAnsi, TRUE);

    if (!NT_SUCCESS(Status))
    {
      goto Error2;
    }

    ServiceName = &ServiceNameUnicode;
  }

  //
  // Convert "struct addrinfo" to the "ADDRINFOEXW".
  //

  PADDRINFOEXW Hints;
  Status = KspUtilAddrInfoToAddrInfoEx((PADDRINFOA)hints, &Hints);

  if (!NT_SUCCESS(Status))
  {
    goto Error3;
  }

  //
  // All data is prepared, call the underlying API.
  //

  PADDRINFOEXW Result;
  Status = KsGetAddrInfo(NodeName, ServiceName, Hints, &Result);

  //
  // Free the memory of the converted "Hints".
  //

  KspUtilFreeAddrInfoEx(Hints);

  if (!NT_SUCCESS(Status))
  {
    goto Error3;
  }

  //
  // Convert the result "ADDRINFOEXW" to the "struct addrinfo".
  //

  Status = KspUtilAddrInfoExToAddrInfo(Result, res);

  //
  // Free the original result.
  //

  KsFreeAddrInfo(Result);

  if (!NT_SUCCESS(Status))
  {
    goto Error3;
  }

  return STATUS_SUCCESS;

Error3:
  RtlFreeUnicodeString(&ServiceNameUnicode);

Error2:
  RtlFreeUnicodeString(&NodeNameUnicode);

Error1:
  return Status;
}

void freeaddrinfo(struct addrinfo *res)
{
  //
  // Call our implementation.
  //

  KspUtilFreeAddrInfo(res);
}

int socket_connection(int domain, int type, int protocol)
{
  NTSTATUS Status;
  PKSOCKET Socket;

  Status = KsCreateConnectionSocket(
    &Socket,
    (ADDRESS_FAMILY)domain,
    (USHORT)type,
    (ULONG)protocol
    );

  if (NT_SUCCESS(Status))
  {
    int sockfd = TO_SOCKETFD(KsIndex++);

    KsArray[FROM_SOCKETFD(sockfd)] = Socket;

    return sockfd;
  }

  return -1;
}

int socket_listen(int domain, int type, int protocol)
{
  NTSTATUS Status;
  PKSOCKET Socket;

  //
  // WskSocket() returns STATUS_PROTOCOL_UNREACHABLE (0xC000023E)
  // when Protocol == 0, so coerce this value to IPPROTO_TCP here.
  //

  Status = KsCreateListenSocket(
    &Socket,
    (ADDRESS_FAMILY)domain,
    (USHORT)type,
    protocol ? (ULONG)protocol : IPPROTO_TCP
    );

  if (NT_SUCCESS(Status))
  {
    int sockfd = TO_SOCKETFD(KsIndex++);

    KsArray[FROM_SOCKETFD(sockfd)] = Socket;

    return sockfd;
  }

  return -1;
}

int socket_datagram(int domain, int type, int protocol)
{
  NTSTATUS Status;
  PKSOCKET Socket;

  Status = KsCreateDatagramSocket(
    &Socket,
    (ADDRESS_FAMILY)domain,
    (USHORT)type,
    (ULONG)protocol
    );

  if (NT_SUCCESS(Status))
  {
    int sockfd = TO_SOCKETFD(KsIndex++);

    KsArray[FROM_SOCKETFD(sockfd)] = Socket;

    return sockfd;
  }

  return -1;
}

int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
  UNREFERENCED_PARAMETER(addrlen);

  NTSTATUS Status;
  PKSOCKET Socket = KsArray[FROM_SOCKETFD(sockfd)];

  Status = KsConnect(Socket, (PSOCKADDR)addr);

  return NT_SUCCESS(Status)
    ? 0
    : -1;
}

int listen(int sockfd, int backlog)
{
  UNREFERENCED_PARAMETER(sockfd);
  UNREFERENCED_PARAMETER(backlog);
  return 0;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
  UNREFERENCED_PARAMETER(addrlen);

  NTSTATUS Status;
  PKSOCKET Socket = KsArray[FROM_SOCKETFD(sockfd)];

  Status = KsBind(Socket, (PSOCKADDR)addr);

  return NT_SUCCESS(Status)
    ? 0
    : -1;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  NTSTATUS Status;
  PKSOCKET Socket = KsArray[FROM_SOCKETFD(sockfd)];

  PKSOCKET NewSocket;
  Status = KsAccept(Socket, &NewSocket, NULL, (PSOCKADDR)addr);
  *addrlen = sizeof(SOCKADDR);

  if (NT_SUCCESS(Status))
  {
    int newsockfd = TO_SOCKETFD(KsIndex++);

    KsArray[FROM_SOCKETFD(newsockfd)] = NewSocket;

    return newsockfd;
  }

  return -1;
}

int send(int sockfd, const void* buf, size_t len, int flags)
{
  NTSTATUS Status;
  PKSOCKET Socket = KsArray[FROM_SOCKETFD(sockfd)];

  ULONG Length = (ULONG)len;
  Status = KsSend(Socket, (PVOID)buf, &Length, (ULONG)flags);

  return NT_SUCCESS(Status)
    ? (int)Length
    : -1;
}

int sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
  UNREFERENCED_PARAMETER(addrlen);

  NTSTATUS Status;
  PKSOCKET Socket = KsArray[FROM_SOCKETFD(sockfd)];

  ULONG Length = (ULONG)len;
  Status = KsSendTo(Socket, (PVOID)buf, &Length, (ULONG)flags, (PSOCKADDR)dest_addr);

  return NT_SUCCESS(Status)
    ? (int)Length
    : -1;
}

int recv(int sockfd, void* buf, size_t len, int flags)
{
  NTSTATUS Status;
  PKSOCKET Socket = KsArray[FROM_SOCKETFD(sockfd)];

  ULONG Length = (ULONG)len;
  Status = KsRecv(Socket, (PVOID)buf, &Length, (ULONG)flags);

  return NT_SUCCESS(Status)
    ? (int)Length
    : -1;
}

int recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
  UNREFERENCED_PARAMETER(addrlen);

  NTSTATUS Status;
  PKSOCKET Socket = KsArray[FROM_SOCKETFD(sockfd)];

  ULONG Length = (ULONG)len;
  Status = KsSendTo(Socket, (PVOID)buf, &Length, (ULONG)flags, (PSOCKADDR)src_addr);
  *addrlen = sizeof(SOCKADDR);

  return NT_SUCCESS(Status)
    ? (int)Length
    : -1;
}

int closesocket(int sockfd)
{
  NTSTATUS Status;
  PKSOCKET Socket = KsArray[FROM_SOCKETFD(sockfd)];

  Status = KsCloseSocket(Socket);

  KsArray[FROM_SOCKETFD(sockfd)] = NULL;

  return NT_SUCCESS(Status)
    ? 0
    : -1;
}
