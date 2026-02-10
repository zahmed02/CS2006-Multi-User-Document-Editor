# Document Collaboration System with Priority-Based Access Control

## Project Overview

This system implements a multi-user document collaboration environment with priority-based access control and synchronization mechanisms. The system allows multiple users to concurrently access and edit shared documents while ensuring data consistency through sophisticated locking mechanisms and priority management.

## Core Components

### 1. **Owner/Admin Program (`owner.c`)**
- **Administrative Control**: Full system management capabilities including user management, document control, and priority override
- **Priority Management**: Highest priority access with ability to interrupt other users
- **Document History**: Complete version control with push/pop history functionality
- **User Management**: Add, remove, update, and list system users with different access levels
- **Real-time Monitoring**: View active users and their current activities

### 2. **User Client Program (`user.c`)**
- **Role-Based Access**: Different access types (read-only, write-only, read-write) based on user permissions
- **Priority-Based Queueing**: Automatic queuing based on user priority levels
- **Responsive Interface**: Menu-driven interface adapting to user access permissions
- **Signal Handling**: Proper response to priority signals from admin/owner
- **Time-Limited Sessions**: Configurable time allocations for editing sessions

### 3. **Shared Synchronization Layer (`shared.c`)**
- **Locking Mechanism**: Implementation of reader-writer locks with priority consideration
- **Shared Memory**: IPC mechanisms for coordinating access between processes
- **Semaphore Management**: Synchronization primitives for access control
- **Signal Processing**: Handling priority signals and time limit notifications
- **Atomic Operations**: Thread-safe operations on shared resources

### 4. **Document Management**
- **Shared Document**: Central text file (`shared_docs.txt`) for collaborative editing
- **Control File**: User database (`shared_doc_control.txt`) storing access permissions and priorities
- **Version History**: Complete change tracking with timestamped snapshots (`history.txt`)
- **Auto-Recovery**: Ability to restore previous document versions from history

## Key Features

### Priority System
- **Three Priority Levels**: Owner (highest), High, Low
- **Priority Override**: Owner can interrupt any user's session with configurable countdown
- **Time Allocation**: Different time limits based on user priority (Owner: 30s, High: 10s, Low: 15s)

### Access Control
- **Three Access Types**: Read-only, Write-only, Read-Write
- **Dynamic Permission Checking**: Real-time validation of user permissions
- **Process Tracking**: Monitor active user processes and their activities

### Synchronization
- **Reader-Writer Locks**: Multiple concurrent readers or single writer
- **Priority Queueing**: Automatic queuing when owner requests access
- **Graceful Handover**: Configurable countdown before forced lock release
- **Editor Integration**: Direct control over external editor processes (nano)

### Document Management
- **Text Editor Integration**: Seamless nano editor integration for document editing
- **Version Control**: Complete history tracking with push/pop functionality
- **Auto-Save Mechanisms**: Graceful save attempts during forced termination
- **Concurrent Access**: Safe multi-user access with proper locking

## File Structure
- `owner.c` - Admin/owner program with full system control
- `user.c` - Client program for regular users
- `shared.h` - Shared header file with declarations and constants
- `shared.c` - Shared synchronization implementation
- `owner.h` - Owner-specific header file
- `shared_docs.txt` - Shared document file
- `shared_doc_control.txt` - User access control database
- `history.txt` - Document version history

## System Architecture
The system uses a client-server-like architecture where the owner program acts as the coordinator and user programs act as clients. All processes communicate through shared memory, semaphores, and signals to coordinate access to the shared document. The locking mechanism ensures data consistency while allowing maximum concurrency through reader-writer locks with priority-based queuing.
