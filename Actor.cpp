/*=============================================================================
Actor

Author and Copyright: Geir Horn, 2017
License: LGPL 3.0
=============================================================================*/

#include "Actor.hpp"

/*=============================================================================

 Actor identification (alias Framework)

=============================================================================*/

// Static members shared by all actors

std::atomic< Theron::Actor::Identification::IDType >
				Theron::Actor::Identification::TotalActorsCreated;

std::unordered_map< std::string, Theron::Actor::Identification * >
				Theron::Actor::Identification::ActorsByName;
				
std::unordered_map< Theron::Actor::Identification::IDType, 
										Theron::Actor::Identification * >
				Theron::Actor::Identification::ActorsByID;

std::mutex Theron::Actor::Identification::InformationAccess;

Theron::Actor * 
Theron::Actor::RemoteIdentity::ThePresentationLayerServer = nullptr;

// -----------------------------------------------------------------------------
// Static functions 
// -----------------------------------------------------------------------------
//
// The static function to return the actor pointer based on a an address will
// throw an invalid argument if the given address is Null

Theron::Actor * Theron::Actor::Identification::GetActor( 
																	 const Theron::Actor::Address & ActorAddress )
{
	if ( ActorAddress )
		return ActorAddress.TheActor->ActorPointer;
	else
		throw std::invalid_argument( "Invalid actor address" );
}

// The first lookup function by name simply acquires the lock and then 
// constructs the address based on the outcome of this lookup. Note that this 
// may result in an invalid address if the given actor name is not found.

Theron::Actor::Address Theron::Actor::Identification::Lookup(
																									const std::string ActorName )
{
	std::lock_guard< std::mutex > Lock( InformationAccess );
	
	auto TheActor = ActorsByName.find( ActorName );
	
	if ( TheActor == ActorsByName.end() )
		return Theron::Actor::Address();
	else
		return Theron::Actor::Address( TheActor->second );
}

// The second lookup function is almost identical except that it uses the ID 
// map to find the actor.

Theron::Actor::Address Theron::Actor::Identification::Lookup(
																														const IDType TheID )
{
	std::lock_guard< std::mutex > Lock( InformationAccess );
	
	auto TheActor = ActorsByID.find( TheID );
	
	if ( TheActor == ActorsByID.end() )
		return Theron::Actor::Address();
	else
		return Theron::Actor::Address( TheActor->second );
}

// The function setting the session layer will throw a logic error if another 
// actor already has claimed the role of a session layer, and the given pointer
// is not null, i.e. that this is invoked by an actor currently acting as the 
// session layer, but about to close.

void Theron::Actor::RemoteIdentity::SetPresentationLayerServer( 
																											Theron::Actor * TheSever )
{
  if ( TheSever == nullptr )
	{
		// The session layer is de-registering and all external references to this 
		// session layer server should be removed - with no session layer server
		// it is not possible to communicate externally and the external references
		// should be invalidated.
	}
	else if ( ThePresentationLayerServer == nullptr  )
		ThePresentationLayerServer = TheSever;
	else 
	{
		std::ostringstream ErrorMessage;
		 
		ErrorMessage << "The session layer server is already set to actor "
								 << ThePresentationLayerServer->ActorID.Name;
								 
	  throw std::logic_error( ErrorMessage.str() );
	}
		
}

// -----------------------------------------------------------------------------
// virtual functions 
// -----------------------------------------------------------------------------

void Theron::Actor::EndpointIdentity::Register( Address * NewAddress )
{
	std::lock_guard< std::recursive_mutex > Lock( AddressAccess );
	Addresses.insert( NewAddress );
}

void Theron::Actor::EndpointIdentity::DeRegister( Address * OldAddress )
{
	std::lock_guard< std::recursive_mutex > Lock( AddressAccess );
	Addresses.erase( OldAddress );
}

void Theron::Actor::RemoteIdentity::Register( Address * NewAddress )
{
	++NumberOfAddresses;
}

void Theron::Actor::RemoteIdentity::DeRegister( Address * OldAddress )
{
	if ( NumberOfAddresses > 0 )
		--NumberOfAddresses;
}

// -----------------------------------------------------------------------------
// Constructor and Destructor 
// -----------------------------------------------------------------------------
//
// The constructor of the identification simply stores the name in 

Theron::Actor::EndpointIdentity::EndpointIdentity( 
		Theron::Actor * TheActor, const std::string & ActorName )
	: Identification( TheActor, ActorName ),
	  Addresses(), AddressAccess()
{
	std::lock_guard< std::mutex > Lock( InformationAccess );
	
	auto Outcome = ActorsByName.emplace( Name, TheActor );
	
	if ( Outcome.second != true )
	{
		std::ostringstream ErrorMessage;
		
		ErrorMessage << "An actor with the name " << Name 
								 << " does already exist!";
		
		throw std::invalid_argument( ErrorMessage.str() );
	}
	
	ActorsByID.emplace( NumericalID, TheActor );
}

// The destructor first invalidates all addresses that references this actor,
// and then removes the actor from the two maps. The lock is acquired only 
// before the second part of the destructor

Theron::Actor::EndpointIdentity::~EndpointIdentity()
{

	// The address registry must be locked for access from this thread. However,
	// as addresses are invalidated, they will de-register which implies that 
	// the lock will be acquired also in the de-registration function. It will 
	// be from the same thread, but it will block unless the mutex accepts 
	// multiple locks.
	
	std::lock_guard< std::recursive_mutex > AddressLock( AddressAccess ); 
	
	// A standard for loop cannot be used to invalidate the addresses since 
	// the invalidation function will call back and remove the address entry 
	// from the address set. Instead, the first element will be invalidated until
	// the set is empty. The assignment is necessary for the compiler to 
	// understand that the returned iterator should not be constant. 
	
	while ( ! Addresses.empty() )
  {
		Address * TheAddress = *(Addresses.begin());
		TheAddress->Invalidate();
	}
	
	std::lock_guard< std::mutex > InformationLock( InformationAccess );
	
	ActorsByName.erase( Name 				);
	ActorsByID.erase  ( NumericalID );
}

// The constructor for the remote identity stores the identity in the registry 
// for actors by name if the Session Layer server is set. Otherwise it will 
// throw a logic error since remote addresses cannot be used without a session
// layer server. 
// 
// It is similar to the end point identity in that it will check that there is 
// no actor by this name already, even though this test should not be necessary
// it is included as an additional precaution. However, it will not store 
// the ID because the numerical ID is valid only on this endpoint, and it 
// will check the availability of the Session Server as a pre-requisite.

Theron::Actor::RemoteIdentity::RemoteIdentity( const std::string & ActorName )
: Identification( ThePresentationLayerServer, ActorName )
{

	if ( ThePresentationLayerServer != nullptr )
  {
		std::lock_guard< std::mutex > Lock( InformationAccess );
		
		auto Outcome = ActorsByName.emplace( Name, this );
		
		if ( Outcome.second != true )
		{
			std::ostringstream ErrorMessage;
			
			ErrorMessage << "An actor with the name " << Name 
									 << " does already exist!";
			
			throw std::invalid_argument( ErrorMessage.str() );
		}
	}
	else
		throw std::logic_error("Remote actor IDs requires a Session Layer Server");
}

/*=============================================================================

 Message handling

=============================================================================*/

// When an actor sends a message to another actor, it will call the 
// enqueue message function on the receiving agent with a pointer to a copy of
// the message to ensure that it does exists also when the message is handed 
// by the receiving actor.
//
// The enqueue function will first append the message to the message queue, and 
// if no postman in running, it will start the thread to dispatch the message 
// to the right handler.

bool Theron::Actor::EnqueueMessage( 
																std::shared_ptr< GenericMessage > & TheMessage )
{
	// Enqueue the message 
	
	QueueGuard.lock();
	Mailbox.push( TheMessage );
	QueueGuard.unlock();
	
	// If the postman is not working, then it will be started. A small detail
	// is that the 'this' pointer must be explicitly passed in order to use 
	// a class member function for a thread.
	
	if ( ! Postman.joinable() )
		Postman = std::thread( &Actor::DispatchMessages, this );
}

/*=============================================================================

 Execution control

=============================================================================*/

// The dispatcher function first obtains a copy of the message at the front of 
// the message queue, and then tries to deliver this to each handler in turn
// Handlers successfully managing this message will be moved forward in the 
// list of handlers according to the transposition rule suggested by Ronald 
// Rivest (1976): "On self-organizing sequential search heuristics", 
// Communications of the ACM, Vol. 19, No. 2, pp. 63-67
//
// If no message handler is available to serve the message the message will 
// be delivered to the default message handler. If no default message handler
// exists, then the message will be handled according to the error policy set.

void Theron::Actor::DispatchMessages( void )
{
	// The main loop will continue as long as there are messages in the queue
	
	while ( ! Mailbox.empty() )
  {
	  // There is an iterator to the current handler, and a flag indicating that 
		// the message has been served by at least one handler.
		
		auto CurrentHandler = MessageHandlers.begin();
		bool MessageServed  = false;
		
		// ...and then use this to loop over all handlers.
		
		while ( CurrentHandler != MessageHandlers.end() )
		{
			// There is a minor problem related to the handler call since invoking the 
			// message handler may create or destroy handlers. A mutex cannot help 
			// since the handler is executing in this thread, and even on the same 
			// stack which means that all operations implicitly made by the handler
			// on the handler list will have terminated when control is returned to 
			// this method. Insertions are not problematic since they will appear at 
			// the end of the list, and will just be included in the continued 
			// iterations here. Deletions are similarly not problematic unless the 
			// handler de-register itself. 
			//
			// In this case it does not help having an iterator to the next element 
			// as there is also no guarantee that that also that pointer will not be 
			// deleted. The only safe way is to ensure that the handler object for 
			// the current handler is not deleted. A copy of the current handler is 
			// therefore made, and its status is set to executing.
			
			auto ExecutingHandler = CurrentHandler;
			(*ExecutingHandler)->SetStatus( GenericHandler::State::Executing );
			
			// Then the handler can process the message, and if this results in the 
			// handler de-registering this handler, it will end with the deleted 
			// state.
						
			if( (*CurrentHandler)->ProcessMessage( Mailbox.front() ) )
		  {
				MessageServed = true;
				
				// Then the list of handlers is optimised by swapping the current 
				// handler with the handler in front unless it is already the first 
				// handler. It is necessary to use a separate swap iterator to ensure 
				// that the current handler iterator points to the next handler not 
				// affected by the swap operation.

				auto SuccessfulHandler = CurrentHandler++;
				
				if( SuccessfulHandler != MessageHandlers.begin() )
					std::iter_swap( SuccessfulHandler, std::prev( SuccessfulHandler ) );
			}
			else	
				++CurrentHandler; // necessary because of the transposition rule 
				
			// The Current Handler is now safely set to a handler that is valid for 
			// the next execution, and the handler just executed can be deleted, or
			// its state can be switched back to normal.
			
			if ( (*ExecutingHandler)->GetStatus() == GenericHandler::State::Deleted )
				MessageHandlers.erase( ExecutingHandler );
			else
				(*ExecutingHandler)->SetStatus( GenericHandler::State::Normal );
		}
		
		// If the message is not served at this point, it should be delivered to 
		// the fall back handler. 
		
		if ( ! MessageServed )
		{
			if ( DefaultHandler )
				DefaultHandler->ProcessMessage( Mailbox.front() );
			else if ( MessageErrorPolicy == MessageError::Throw )
		  {
				std::ostringstream ErrorMessage;
				
				ErrorMessage << "No message handler for the message " 
										 << typeid( *(Mailbox.front()) ).name() 
										 << " and no default message handler!";
										 
			  throw std::logic_error( ErrorMessage.str() );
			}
		}
		
		// The message is fully handled, and it can be popped from the queue and 
		// thereby prepare the queue for processing the next message.
		
		QueueGuard.lock();
		Mailbox.pop();
		QueueGuard.unlock();
	}
}

